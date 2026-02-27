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