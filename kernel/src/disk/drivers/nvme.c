#include "nvme.h"
#include "status.h"
#include "memory/heap/kheap.h"
#include "memory/paging/paging.h"
#include "memory/memory.h"
#include "kernel.h"
#include "io/pci.h"

static uint32_t nvme_disk_driver_read_reg(struct disk *disk, uint32_t off);
static void nvme_disk_driver_write_reg(struct disk *disk, uint32_t off, uint32_t value);
static void nvme_disk_driver_unmount(struct disk *disk);

static inline uint32_t nvme_read32(struct nvme_disk_driver_private *priv, uint32_t off)
{
	return *((volatile uint32_t *)(priv->base_address_nvme + off));
}

static inline void nvme_write32(struct nvme_disk_driver_private *priv, uint32_t off, uint32_t value)
{
	*((volatile uint32_t *)(priv->base_address_nvme + off)) = value;
}

static inline uint64_t nvme_read64(struct nvme_disk_driver_private *priv, uint32_t off)
{
	uint32_t lo = nvme_read32(priv, off);
	uint32_t hi = nvme_read32(priv, off + 4);
	return ((uint64_t)hi << 32) | lo;
}

static inline void nvme_write64(struct nvme_disk_driver_private *priv, uint32_t off, uint64_t value)
{
	nvme_write32(priv, off, (uint32_t)(value & 0xFFFFFFFFu));
	nvme_write32(priv, off + 4, (uint32_t)(value >> 32));
}

static int nvme_admin_cmd_raw(struct disk *disk, uint8_t opcode, uint32_t nsid, uint64_t prp1, uint32_t cdw10, uint32_t cdw11)
{
	struct nvme_disk_driver_private *p = disk_private_data_driver(disk);
	struct nvme_submission_queue_entry cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = NVME_COMMAND_BITS_BUILD(opcode, 0, 0, 0);
	cmd.nsid = nsid;
	cmd.data_ptr1 = (uint32_t)(prp1 & 0xFFFFFFFFu);
	cmd.data_ptr2 = (uint32_t)(prp1 >> 32);
	cmd.command_cdw[0] = cdw10;
	cmd.command_cdw[1] = cdw11;

	// post the command to the admin submission queue
	struct nvme_submission_queue_entry *sqe = p->submission_queue.ptr + p->submission_queue.tail;
	memcpy(sqe, &cmd, sizeof(cmd));
	uint16_t new_tail = (uint16_t)(p->submission_queue.tail + 1u);
	if (new_tail >= p->submission_queue.size)
	{
		new_tail = 0;
	}

	p->submission_queue.tail = new_tail;
	nvme_disk_driver_write_reg(disk, NVME_SQTDBL_OFFSET(0, p->doorbell_stride), p->submission_queue.tail);

	// poll for completion
	struct nvme_completion_queue_entry *cqe = p->completion_queue.ptr + p->completion_queue.head;
	for (int i = 0; i < 1000000; i++)
	{
		uint32_t st = cqe->status_phase_and_command_identifier;
		if (((st >> 16) & 1u) == p->admin_cq_phase)
		{
			break; // command completed
		}

		__asm__ volatile("pause");
	}

	if (((cqe->status_phase_and_command_identifier >> 16) & 1u) != p->admin_cq_phase)
	{
		return -ETIMEOUT; // command did not complete in time
	}

	uint32_t st = cqe->status_phase_and_command_identifier;
	uint16_t status = (st >> 17) & 0x7FFFu;

	uint16_t new_head = (uint16_t)(p->completion_queue.head + 1u);
	if (new_head >= p->completion_queue.size)
	{
		new_head = 0;
		p->admin_cq_phase ^= 1; // toggle phase bit
	}

	p->completion_queue.head = new_head;
	nvme_disk_driver_write_reg(disk, NVME_CQTDBL_OFFSET(0, p->doorbell_stride), p->completion_queue.head);

	return status == 0 ? 0 : -EIO;
}

static int nvme_create_io_cq(struct disk *disk, uint16_t qid, uint16_t qsize, void *cq_virt)
{
	uint32_t cdw10 = ((uint32_t)(qsize - 1) << 16) | qid;
	uint32_t cdw11 = 0x01;
	return nvme_admin_cmd_raw(disk, 0x05, 0, (uint64_t)cq_virt, cdw10, cdw11);
}

static int nvme_create_io_sq(struct disk *disk, uint16_t qid, uint16_t qsize, void *sq_virt, uint16_t cqid)
{
	uint32_t cdw10 = ((uint32_t)(qsize - 1) << 16) | qid;
	uint32_t cdw11 = ((uint32_t)cqid << 16) | 0x01;
	return nvme_admin_cmd_raw(disk, 0x01, 0, (uint64_t)sq_virt, cdw10, cdw11);
}

static struct nvme_disk_driver_private *nvme_pci_private_new(struct pci_device *dev)
{
	struct nvme_disk_driver_private *priv = kzalloc(sizeof(struct nvme_disk_driver_private));
	if (!priv)
	{
		return NULL;
	}

	priv->device = dev;
	return priv;
}

static void nvme_pci_device_private_free(struct nvme_disk_driver_private *priv)
{
	if (priv)
	{
		kfree(priv);
	}
}

static bool nvme_pci_device(struct pci_device *dev)
{
	return dev->class.base == NVME_PCI_BASE_CLASS && dev->class.subclass == NVME_PCI_SUBCLASS;
}

static uint32_t nvme_disk_driver_read_reg(struct disk *disk, uint32_t off)
{
	struct nvme_disk_driver_private *priv = disk_private_data_driver(disk);
	if (!priv)
	{
		panic("nvme_disk_driver_read_reg: disk private data is NULL\n");
	}

	return nvme_read32(priv, off);
}

static void nvme_disk_driver_write_reg(struct disk *disk, uint32_t off, uint32_t value)
{
	struct nvme_disk_driver_private *priv = disk_private_data_driver(disk);
	if (!priv)
	{
		panic("nvme_disk_driver_write_reg: disk private data is NULL\n");
	}

	nvme_write32(priv, off, value);
}

static void nvme_map_mmio_once(struct nvme_disk_driver_private *priv)
{
	uintptr_t base = (uintptr_t)priv->base_address_nvme;
	uint32_t sz = priv->device->bars[0].size ? priv->device->bars[0].size : 0x2000; // default to 8KB if size is not specified
	const uint32_t flags = PAGING_IS_PRESENT | PAGING_IS_WRITEABLE | PAGING_CACHE_DISABLED;
	for (uintptr_t off = 0; off < sz; off += 0x1000)
	{
		paging_map(kernel_desc(), (void *)(base + off), (void *)(base + off), flags);
	}
}

static inline struct nvme_submission_queue_entry *nvme_submission_queue_tail(struct nvme_disk_driver_private *priv)
{
	return priv->submission_queue.ptr + priv->submission_queue.tail;
}

static inline struct nvme_completion_queue_entry *nvme_completion_queue_head(struct nvme_disk_driver_private *priv)
{
	return priv->completion_queue.ptr + priv->completion_queue.head;
}

static int nvme_admin_submission_queue_init(struct disk *disk)
{
	struct nvme_disk_driver_private *priv = disk_private_data_driver(disk);
	return priv->submission_queue.ptr ? 0 : -ENOMEM;
}

static int nvme_admin_completion_queue_init(struct disk *disk)
{
	struct nvme_disk_driver_private *priv = disk_private_data_driver(disk);
	return priv->completion_queue.ptr ? 0 : -ENOMEM;
}

static uint8_t nvme_cap_doorbell_stride(struct disk *disk)
{
	uint32_t cap_hi = nvme_disk_driver_read_reg(disk, NVME_BASE_REGISTER_CAP + 4);
	return (uint8_t)(cap_hi & 0xFu);
}

static int nvme_admin_send_command(struct disk *disk, uint8_t opcode, uint32_t nsid, void *data, uint64_t lba, uint16_t num_blocks, struct nvme_completion_queue_entry **completion_out)
{
	struct nvme_disk_driver_private *priv = disk_private_data_driver(disk);
	struct nvme_submission_queue_entry cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = NVME_COMMAND_BITS_BUILD(opcode, 0, 0, 0);
	cmd.nsid = nsid;
	cmd.data_ptr1 = (uint32_t)((uintptr_t)data & 0xFFFFFFFFu);
	cmd.data_ptr2 = (uint32_t)((uintptr_t)data >> 32);
	cmd.command_cdw[0] = (uint32_t)(lba & 0xFFFFFFFFu);
	cmd.command_cdw[1] = (uint32_t)(lba >> 32);
	cmd.command_cdw[2] = (num_blocks == 0) ? 0 : (num_blocks - 1u); // 0 means 1 block, so subtract 1 from num_blocks

	// post the command to the admin submission queue
	struct nvme_submission_queue_entry *sqe = nvme_submission_queue_tail(priv);
	memcpy(sqe, &cmd, sizeof(cmd));
	uint16_t new_tail = (uint16_t)(priv->submission_queue.tail + 1u);
	if (new_tail >= priv->submission_queue.size)
	{
		new_tail = 0;
	}

	priv->submission_queue.tail = new_tail;
	nvme_disk_driver_write_reg(disk, NVME_SQTDBL_OFFSET(0, priv->doorbell_stride), priv->submission_queue.tail);

	// poll for completion
	struct nvme_completion_queue_entry *cqe = nvme_completion_queue_head(priv);
	for (int i = 0; i < 1000000; i++)
	{
		uint32_t st = cqe->status_phase_and_command_identifier;
		if (((st >> 16) & 1u) == priv->admin_cq_phase)
		{
			break; // command completed
		}

		__asm__ volatile("pause");
	}

	if (((cqe->status_phase_and_command_identifier >> 16) & 1u) != priv->admin_cq_phase)
	{
		return -ETIMEOUT; // command did not complete in time
	}

	uint32_t st = cqe->status_phase_and_command_identifier;
	uint16_t status = (st >> 17) & 0x7FFFu;

	uint16_t new_head = (uint16_t)(priv->completion_queue.head + 1u);
	if (new_head >= priv->completion_queue.size)
	{
		new_head = 0;
		priv->completion_queue.phase ^= 1; // toggle phase bit
	}

	priv->completion_queue.head = new_head;
	nvme_disk_driver_write_reg(disk, NVME_CQTDBL_OFFSET(0, priv->doorbell_stride), priv->completion_queue.head);
	if (completion_out)
	{
		*completion_out = cqe;
	}

	return status == 0 ? 0 : -EIO;
}

static void *nvme_pci_mmio_base(struct pci_device *dev)
{
	uint64_t lo = ((uint64_t)dev->bars[0].addr) & 0xFFFFFFFF0ull;
	uint64_t hi = (uint64_t)dev->bars[1].addr;
	return (void *)(uintptr_t)(hi << 32 | lo);
}

static int nvme_disk_driver_mount_for_device(struct disk_driver *driver, struct pci_device *dev)
{
	int res = 0;
	pci_enable_bus_master(dev);

	struct nvme_disk_driver_private *priv = nvme_pci_private_new(dev);
	if (!priv)
	{
		return -ENOMEM;
	}

	priv->base_address_nvme = nvme_pci_mmio_base(dev);
	nvme_map_mmio_once(priv);

	struct disk *disk = NULL;
	res = disk_create_new(driver, NULL, MYOS_DISK_TYPE_REAL, 0, 0, NVME_SECTOR_SIZE, priv, &disk);
	if (res < 0)
	{
		nvme_pci_device_private_free(priv);
		return res;
	}

	uint32_t cc = nvme_disk_driver_read_reg(disk, NVME_BASE_REGISTER_CC);
	cc &= ~1u; // clear the enable bit
	nvme_disk_driver_write_reg(disk, NVME_BASE_REGISTER_CC, cc);
	for (int i = 0; i < 5000; i++)
	{
		if ((nvme_disk_driver_read_reg(disk, NVME_BASE_REGISTER_CSTS) & 1u) == 0)
		{
			break; // controller is ready
		}

		__asm__ volatile("pause");
	}

	if ((nvme_disk_driver_read_reg(disk, NVME_BASE_REGISTER_CSTS) & 1u) != 0)
	{
		nvme_disk_driver_unmount(disk);
		return -ETIMEOUT; // controller did not become ready in time
	}

	uint64_t cap = nvme_read64(priv, NVME_BASE_REGISTER_CAP);
	uint16_t mqes = (uint16_t)((cap & 0xFFFFu) + 1u); // maximum queue entries supported by the controller
	priv->doorbell_stride = (uint8_t)((cap >> 32) & 0xFu);

	// setup admin submission queue
	priv->submission_queue.size = NVME_ADMIN_SUBMISSION_QUEUE_TOTAL_ENTRIES <= mqes ? NVME_ADMIN_SUBMISSION_QUEUE_TOTAL_ENTRIES : mqes;
	priv->completion_queue.size = NVME_ADMIN_COMPLETION_QUEUE_TOTAL_ENTRIES <= mqes ? NVME_ADMIN_COMPLETION_QUEUE_TOTAL_ENTRIES : mqes;
	priv->submission_queue.tail = 0;
	priv->completion_queue.head = 0;
	priv->submission_queue.ptr = kzalloc(sizeof(struct nvme_submission_queue_entry) * priv->submission_queue.size);
	priv->completion_queue.ptr = kzalloc(sizeof(struct nvme_completion_queue_entry) * priv->completion_queue.size);
	if (!priv->submission_queue.ptr || !priv->completion_queue.ptr)
	{
		nvme_disk_driver_unmount(disk);
		return -ENOMEM;
	}

	uint32_t aqa = ((uint32_t)(priv->completion_queue.size - 1) << 16) | ((priv->submission_queue.size - 1) << 0);
	nvme_disk_driver_write_reg(disk, NVME_BASE_REGISTER_AQA, aqa);
	nvme_write64(priv, NVME_BASE_REGISTER_ASQ, (uint64_t)(uintptr_t)priv->submission_queue.ptr);
	nvme_write64(priv, NVME_BASE_REGISTER_ACQ, (uint64_t)(uintptr_t)priv->completion_queue.ptr);

	cc = 0;
	cc |= (0u << 7);  // MPS=0 means 16 bytes per PRP entry, which is what we are using
	cc |= (0u << 4);  // CSS=0 means the command set is the NVM command set
	cc |= (6u << 16); // IOSQES submission queue entry size = 64 bytes
	cc |= (4u << 20); // IOCQES completion queue entry size = 16 bytes
	cc |= 1u;		  // set the enable bit to enable the controller
	nvme_disk_driver_write_reg(disk, NVME_BASE_REGISTER_CC, cc);

	// wait for the controller to set the ready bit
	for (int i = 0; i < 5000; i++)
	{
		if ((nvme_disk_driver_read_reg(disk, NVME_BASE_REGISTER_CSTS) & 1u) == 1u)
		{
			break; // controller is ready
		}

		__asm__ volatile("pause");
	}

	if ((nvme_disk_driver_read_reg(disk, NVME_BASE_REGISTER_CSTS) & 1u) != 1u)
	{
		nvme_disk_driver_unmount(disk);
		return -ETIMEOUT; // controller did not become ready in time
	}

	priv->admin_cq_phase = 1; // initialize the admin completion queue phase to 1
	priv->nsid = 1;

	// create IO queues
	const uint16_t io_entries = mqes < 64 ? mqes : 64;
	priv->io_submission_queue.size = io_entries;
	priv->io_completion_queue.size = io_entries;
	priv->io_submission_queue.tail = 0;
	priv->io_completion_queue.head = 0;
	priv->io_completion_queue.phase = 1; // initialize the IO completion queue phase to 1

	priv->io_submission_queue.ptr = kzalloc(sizeof(struct nvme_submission_queue_entry) * io_entries);
	priv->io_completion_queue.ptr = kzalloc(sizeof(struct nvme_completion_queue_entry) * io_entries);
	if (!priv->io_submission_queue.ptr || !priv->io_completion_queue.ptr)
	{
		nvme_disk_driver_unmount(disk);
		return -ENOMEM;
	}

	res = nvme_create_io_cq(disk, 1, io_entries, priv->io_completion_queue.ptr);
	if (res < 0)
	{
		nvme_disk_driver_unmount(disk);
		return res;
	}

	res = nvme_create_io_sq(disk, 1, io_entries, priv->io_submission_queue.ptr, 1);
	if (res < 0)
	{
		nvme_disk_driver_unmount(disk);
		return res;
	}

	return 0;
}

static int nvme_io_submit_and_poll(struct disk *disk, uint8_t opcode, uint64_t lba, uint16_t num_blocks, void *buf)
{
	struct nvme_disk_driver_private *priv = disk_private_data_driver(disk);
	const uint32_t page_size = 4096;
	uint32_t bytes = (uint32_t)num_blocks * NVME_SECTOR_SIZE;
	uintptr_t addr = (uintptr_t)buf;

	// PRP calculation
	uint32_t first_span = page_size - (uint32_t)(addr & (page_size - 1)); // bytes until the end of the first page
	if (first_span >= bytes)
	{
		first_span = bytes; // if the data fits in the first page, adjust the first span
	}

	uint64_t prp1 = (uint64_t)addr;
	uint64_t prp2 = 0;
	if (bytes > first_span)
	{
		uint32_t remain = bytes - first_span;
		if (remain > page_size)
		{
			remain = page_size; // we only support up to 2 PRPs, so if the remaining data exceeds one page, we limit it to one page
		}

		prp2 = (uint64_t)(addr + first_span);
		uint32_t described = first_span + remain;
		num_blocks = (uint16_t)((described + NVME_SECTOR_SIZE - 1) / NVME_SECTOR_SIZE); // recalculate num_blocks based on the actual described bytes
	}

	// build the command
	struct nvme_submission_queue_entry cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.command = NVME_COMMAND_BITS_BUILD(opcode, 0, 0, 0);
	cmd.nsid = priv->nsid;
	cmd.data_ptr1 = (uint32_t)(prp1 & 0xFFFFFFFFu);
	cmd.data_ptr2 = (uint32_t)(prp1 >> 32);
	cmd.data_ptr3 = (uint32_t)(prp2 & 0xFFFFFFFFu);
	cmd.data_ptr4 = (uint32_t)(prp2 >> 32);
	cmd.command_cdw[0] = (uint32_t)(lba & 0xFFFFFFFFu);
	cmd.command_cdw[1] = (uint32_t)(lba >> 32);
	cmd.command_cdw[2] = (uint32_t)(num_blocks - 1u);

	// post the command to the IO submission queue
	struct nvme_submission_queue_entry *sqe = priv->io_submission_queue.ptr + priv->io_submission_queue.tail;
	memcpy(sqe, &cmd, sizeof(cmd));
	uint16_t new_tail = (uint16_t)(priv->io_submission_queue.tail + 1u);
	if (new_tail >= priv->io_submission_queue.size)
	{
		new_tail = 0;
	}

	priv->io_submission_queue.tail = new_tail;
	nvme_disk_driver_write_reg(disk, NVME_SQTDBL_OFFSET(1, priv->doorbell_stride), priv->io_submission_queue.tail);

	// poll for completion
	struct nvme_completion_queue_entry *cqe = priv->io_completion_queue.ptr + priv->io_completion_queue.head;
	for (int i = 0; i < 1000000; i++)
	{
		uint32_t st = cqe->status_phase_and_command_identifier;
		if (((st >> 16) & 1u) == priv->io_completion_queue.phase)
		{
			break; // command completed
		}

		__asm__ volatile("pause");
	}

	if (((cqe->status_phase_and_command_identifier >> 16) & 1u) != priv->io_completion_queue.phase)
	{
		return -ETIMEOUT; // command did not complete in time
	}

	uint32_t st2 = cqe->status_phase_and_command_identifier;
	uint16_t status = (uint16_t)(st2 >> 17) & 0x7FFFu;
	uint16_t new_head = (uint16_t)(priv->io_completion_queue.head + 1u);
	if (new_head >= priv->io_completion_queue.size)
	{
		new_head = 0;
		priv->io_completion_queue.phase ^= 1; // toggle phase bit
	}

	priv->io_completion_queue.head = new_head;
	nvme_disk_driver_write_reg(disk, NVME_CQTDBL_OFFSET(1, priv->doorbell_stride), priv->io_completion_queue.head);

	return status == 0 ? 0 : -EIO;
}

static int nvme_disk_driver_mount(struct disk_driver *driver)
{
	int res = 0;
	size_t total_pci = pci_device_count();
	for (size_t i = 0; i < total_pci; i++)
	{
		struct pci_device *dev = NULL;
		res = pci_device_get(i, &dev);
		if (res < 0)
		{
			break;
		}

		if (nvme_pci_device(dev))
		{
			res = nvme_disk_driver_mount_for_device(driver, dev);
			if (res < 0)
			{
				break;
			}
		}
	}

	return res;
}

static void nvme_disk_driver_unmount(struct disk *disk)
{
	struct nvme_disk_driver_private *priv = disk_private_data_driver(disk);
	if (priv)
	{
		nvme_pci_device_private_free(priv);
	}
}

static int nvme_disk_driver_read(struct disk *disk, uint64_t lba, uint32_t total_sectors, void *buf)
{
	struct disk *hw = disk_hardware_disk(disk);
	if (!hw)
	{
		hw = disk;
	}

	int remaining_sectors = total_sectors;
	uint64_t current_lba = lba;
	uint8_t *current_buf = (uint8_t *)buf;
	while (remaining_sectors > 0)
	{
		uint16_t nlb = (remaining_sectors > 16) ? 16 : (uint16_t)remaining_sectors; // we limit each command to at most 16 sectors to avoid PRP complications
		int rc = nvme_io_submit_and_poll(hw, NVME_OPCODE_READ, current_lba, nlb, current_buf);
		if (rc < 0)
		{
			return rc;
		}

		current_lba += nlb;
		current_buf += (size_t)nlb * NVME_SECTOR_SIZE;
		remaining_sectors -= nlb;
	}

	return 0;
}

static int nvme_disk_driver_write(struct disk *disk, uint64_t lba, uint32_t total_sectors, const void *buf)
{
	struct disk *hw = disk_hardware_disk(disk);
	if (!hw)
	{
		hw = disk;
	}

	int remaining_sectors = total_sectors;
	uint64_t current_lba = lba;
	const uint8_t *current_buf = (const uint8_t *)buf;
	while (remaining_sectors > 0)
	{
		uint16_t nlb = (remaining_sectors > 16) ? 16 : (uint16_t)remaining_sectors; // we limit each command to at most 16 sectors to avoid PRP complications
		int rc = nvme_io_submit_and_poll(hw, NVME_OPCODE_WRITE, current_lba, nlb, (void *)current_buf);
		if (rc < 0)
		{
			return rc;
		}

		current_lba += nlb;
		current_buf += (size_t)nlb * NVME_SECTOR_SIZE;
		remaining_sectors -= nlb;
	}

	return 0;
}

static int nvme_disk_driver_mount_partition(struct disk *disk, uint64_t starting_lba, uint64_t ending_lba, struct disk **partition_disk_out)
{
	return disk_create_new(disk->driver, disk->hardware_disk, MYOS_DISK_TYPE_PARTITION, starting_lba, ending_lba, disk->sector_size, NULL, partition_disk_out);
}

static struct disk_driver nvme_driver = {
	.name = "NVME",
	.functions = {
		.loaded = NULL,
		.unloaded = NULL,
		.mount = nvme_disk_driver_mount,
		.unmount = nvme_disk_driver_unmount,
		.read = nvme_disk_driver_read,
		.write = nvme_disk_driver_write,
		.mount_partition = nvme_disk_driver_mount_partition,
	},
};

struct disk_driver *nvme_driver_init(void)
{
	return &nvme_driver;
}