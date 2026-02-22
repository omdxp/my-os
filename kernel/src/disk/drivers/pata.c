#include "pata.h"
#include "status.h"
#include "disk/disk.h"
#include "io/io.h"
#include "io/pci.h"
#include "memory/heap/kheap.h"

int pata_address(int base, int offset)
{
	return base + offset;
}

static bool pata_pci_device_present()
{
	size_t total_pci = pci_device_count();
	for (size_t i = 0; i < total_pci; i++)
	{
		struct pci_device *device = NULL;
		if (pci_device_get(i, &device) < 0 || !device)
		{
			continue;
		}

		if (device->class.base == PATA_PCI_BASE_CLASS && device->class.subclass == PATA_PCI_SUBCLASS)
		{
			return true;
		}
	}

	return false;
}

struct pata_driver_private_data *pata_private_new(PATA_DISK_DRIVE type, struct disk *primary_disk)
{
	struct pata_driver_private_data *primary_drive_data = kzalloc(sizeof(struct pata_driver_private_data));
	if (!primary_drive_data)
	{
		return NULL;
	}

	primary_drive_data->disk_drive = PATA_PRIMARY_DRIVE;
	primary_drive_data->real_disk = primary_disk;

	return primary_drive_data;
}

int pata_disk_base_drive_address(struct disk *disk, int offset)
{
	int drive_address = PATA_INVALID_BASE_ADDRESS;
	struct pata_driver_private_data *private_data = disk_private_data_driver(disk);
	if (!private_data)
	{
		goto out;
	}

	if (private_data->disk_drive == PATA_PRIMARY_DRIVE)
	{
		drive_address = PATA_PRIMARY_DRIVE_BASE_ADDRESS + offset;
	}
	else if (private_data->disk_drive == PATA_SECONDARY_DRIVE)
	{
		drive_address = PATA_SECONDARY_DRIVE_BASE_ADDRESS + offset;
	}

	if (drive_address == PATA_INVALID_BASE_ADDRESS)
	{
		goto out;
	}

out:
	return drive_address;
}

int pata_disk_ctrl_drive_address(struct disk *disk, int offset)
{
	int drive_address = PATA_INVALID_BASE_ADDRESS;
	struct pata_driver_private_data *private_data = disk_private_data_driver(disk);
	if (!private_data)
	{
		goto out;
	}

	if (private_data->disk_drive == PATA_PRIMARY_DRIVE)
	{
		drive_address = PATA_PRIMARY_DRIVE_CTRL_ADDRESS + offset;
	}
	else if (private_data->disk_drive == PATA_SECONDARY_DRIVE)
	{
		drive_address = PATA_SECONDARY_DRIVE_CTRL_ADDRESS + offset;
	}

	if (drive_address == PATA_INVALID_BASE_ADDRESS)
	{
		goto out;
	}

out:
	return drive_address;
}

int pata_disk_read_sector(struct disk *disk, uint64_t lba, uint32_t total_sectors, void *buf)
{
	int base_address = pata_disk_base_drive_address(disk, 0x00);
	while (insb(pata_address(base_address, 0x07)) & 0x80) // wait until the drive is not busy
		;
	outb(pata_address(base_address, 0x06), 0xE0 | ((lba >> 24) & 0x0F));		 // set drive and head
	outb(pata_address(base_address, 0x02), (unsigned char)total_sectors);		 // set total sectors to read
	outb(pata_address(base_address, 0x03), (unsigned char)(lba & 0xFF));		 // set LBA low byte
	outb(pata_address(base_address, 0x04), (unsigned char)((lba >> 8) & 0xFF));	 // set LBA mid byte
	outb(pata_address(base_address, 0x05), (unsigned char)((lba >> 16) & 0xFF)); // set LBA high byte
	outb(pata_address(base_address, 0x07), 0x20);								 // send read command

	unsigned short *ptr = (unsigned short *)buf;
	for (uint32_t i = 0; i < total_sectors; i++)
	{
		while (insb(pata_address(base_address, 0x07)) & 0x80) // wait until the drive is not busy
			;

		// check for error bit
		char status = insb(pata_address(base_address, 0x07));
		if (status & 0x01)
		{
			return -EIO;
		}

		while (!(insb(pata_address(base_address, 0x07)) & 0x08)) // wait until data is ready
			;

		for (int word = 0; word < 256; word++) // read 256 words (512 bytes) per sector
		{
			*ptr++ = insw(pata_address(base_address, 0x00));
		}
	}

	return 0;
}

int pata_driver_read(struct disk *disk, uint64_t lba, uint32_t total_sectors, void *buf)
{
	int res = 0;
	struct disk *hardware_disk = disk_hardware_disk(disk);
	res = pata_disk_read_sector(hardware_disk, lba, total_sectors, buf);
	return res;
}

int pata_driver_write(struct disk *disk, uint64_t lba, uint32_t total_sectors, const void *buf)
{
	int res = -EUNIMP;
	return res;
}

int pata_driver_mount(struct disk_driver *driver)
{
	int res = 0;
	if (!pata_pci_device_present())
	{
		res = -ENOENT;
		goto out;
	}

	struct pata_driver_private_data *primary_private_data = pata_private_new(PATA_PRIMARY_DRIVE, NULL);
	if (!primary_private_data)
	{
		res = -ENOMEM;
		goto out;
	}

	primary_private_data->disk_drive = PATA_PRIMARY_DRIVE;
	res = disk_create_new(driver, NULL, MYOS_DISK_TYPE_REAL, 0, 0, KERNEL_PATA_SECTOR_SIZE, primary_private_data, &primary_private_data->real_disk);
	if (res < 0)
	{
		goto out;
	}

	// TODO: detect if secondary drive exists and create disk for it as well

out:
	return res;
}

void pata_driver_unmount(struct disk *disk)
{
	struct pata_driver_private_data *private_data = disk_private_data_driver(disk);
	if (private_data)
	{
		kfree(private_data);
		disk->driver_private = NULL;
	}
}

int pata_driver_loaded(struct disk_driver *driver)
{
	// nothing to do here since we never unload the pata driver
	return 0;
}

void pata_driver_unloaded(struct disk_driver *driver)
{
	// nothing to do here since we never unload the pata driver
}

int pata_driver_mount_partition(struct disk *disk, uint64_t starting_lba, uint64_t ending_lba, struct disk **partition_disk_out)
{
	int res = 0;
	struct pata_driver_private_data *private_data = pata_private_new(PATA_PARTITION_DRIVE, disk);
	if (!private_data)
	{
		res = -ENOMEM;
		goto out;
	}

	res = disk_create_new(disk->driver, disk, MYOS_DISK_TYPE_PARTITION, starting_lba, ending_lba, disk->sector_size, private_data, partition_disk_out);
	if (res < 0)
	{
		goto out;
	}

out:
	return res;
}

struct disk_driver pata_driver = {
	.name = "PATA",
	.functions =
		{
			.loaded = pata_driver_loaded,
			.unloaded = pata_driver_unloaded,
			.mount = pata_driver_mount,
			.unmount = pata_driver_unmount,
			.read = pata_driver_read,
			.write = pata_driver_write,
			.mount_partition = pata_driver_mount_partition,
		},
};

struct disk_driver *pata_driver_init()
{
	return &pata_driver;
}