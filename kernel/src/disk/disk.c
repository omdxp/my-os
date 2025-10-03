#include "disk.h"
#include "io/io.h"
#include "memory/memory.h"
#include "config.h"
#include "status.h"
#include "lib/vector.h"
#include "string/string.h"
#include "memory/heap/kheap.h"

struct vector *disk_vector = NULL;	 // vector of all disks in the system
struct disk *disk = NULL;			 // pointer to the primary disk
struct disk *primary_fs_disk = NULL; // the disk that contains the primary filesystem

int disk_read_sector(int lba, int total, void *buf)
{
	// wait until not busy
	while (insb(0x1F7) & 0x80)
		;

	outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
	outb(0x1F2, (unsigned char)total);
	outb(0x1F3, (unsigned char)(lba & 0xff));
	outb(0x1F4, (unsigned char)(lba >> 8) & 0xff);
	outb(0x1F5, (unsigned char)((lba >> 16) & 0xff));
	outb(0x1F7, 0x20);

	unsigned short *ptr = (unsigned short *)buf;
	for (int i = 0; i < total; i++)
	{
		// wait until not busy
		while (insb(0x1F7) & 0x80)
			;

		// check for errors
		char status = insb(0x1F7);
		if (status & 0x01)
		{
			return -EIO;
		}

		// wait for buffer to be ready
		while (!(insb(0x1F7) & 0x08))
			;

		// copy from hard disk to memory
		for (int word = 0; word < 256; word++)
		{
			*ptr++ = insw(0x1F0);
		}
	}

	return 0;
}

int disk_create_new(int type, int starting_lba, int ending_lba, size_t sector_size, struct disk **out_disk)
{
	int res = 0;
	struct disk *disk = kzalloc(sizeof(struct disk));
	if (!disk)
	{
		res = -ENOMEM;
		goto out;
	}

	disk->type = type;
	disk->sector_size = sector_size;
	disk->starting_lba = starting_lba;
	disk->ending_lba = ending_lba;
	disk->id = vector_count(disk_vector);
	disk->filesystem = fs_resolve(disk);
	if (disk->filesystem)
	{
		char fs_name[11] = {0};
		char primary_drive_fs_name[11] = {0};
		strncpy(primary_drive_fs_name, MYOS_KERNEL_FILESYSTEM_NAME, strlen(MYOS_KERNEL_FILESYSTEM_NAME));
		disk->filesystem->volume_name(disk->fs_private, fs_name, sizeof(fs_name));
		if (strncmp(fs_name, primary_drive_fs_name, strlen(primary_drive_fs_name)) == 0)
		{
			primary_fs_disk = disk;
		}
	}

	if (out_disk)
	{
		*out_disk = disk;
	}

	res = vector_push(disk_vector, disk);
	if (res < 0)
	{
		goto out;
	}

out:
	return res;
}

int disk_search_and_init()
{
	int res = 0;
	disk_vector = vector_new(sizeof(struct disk *), 4, 0);
	if (!disk_vector)
	{
		res = -ENOMEM;
		goto out;
	}

	res = disk_create_new(MYOS_DISK_TYPE_REAL, 0, 0, MYOS_SECTOR_SIZE, &disk);
	if (res < 0)
	{
		goto out;
	}

out:
	return res;
}

struct disk *disk_primary()
{
	return disk;
}

struct disk *disk_primary_fs_disk()
{
	return primary_fs_disk;
}

struct disk *disk_get(int index)
{
	size_t total_disks = vector_count(disk_vector);
	if ((size_t)index >= total_disks)
	{
		return NULL;
	}

	struct disk *disk = NULL;
	vector_at(disk_vector, index, &disk, sizeof(struct disk *));
	return disk;
}

int disk_read_block(struct disk *idisk, unsigned int lba, int total, void *buf)
{
	size_t absolute_lba = idisk->starting_lba + lba;
	size_t absolute_ending_lba = absolute_lba + total;
	if (absolute_ending_lba > idisk->ending_lba)
	{
		if (idisk->starting_lba != 0 && idisk->ending_lba != 0) // out of bounds check only if not primary disk
		{
			return -EIO;
		}
	}

	return disk_read_sector(absolute_lba, total, buf);
}
