#include "disk.h"
#include "io/io.h"
#include "memory/memory.h"
#include "config.h"
#include "status.h"
#include "lib/vector.h"
#include "string/string.h"
#include "memory/heap/kheap.h"
#include "driver.h"
#include "disk/streamer.h"

struct vector *disk_vector = NULL;	 // vector of all disks in the system
struct disk *disk = NULL;			 // pointer to the primary disk
struct disk *primary_fs_disk = NULL; // the disk that contains the primary filesystem

struct disk *disk_hardware_disk(struct disk *disk)
{
	return disk->hardware_disk;
}

int disk_create_partition(struct disk *disk, uint64_t starting_lba, uint64_t ending_lba, struct disk **partition_disk_out)
{
	return disk_driver_mount_partition(disk->driver, disk, starting_lba, ending_lba, partition_disk_out);
}

int disk_filesystem_mount(struct disk *disk)
{
	disk->filesystem = fs_resolve(disk);
	if (disk->filesystem)
	{
		char fs_name[11] = {0};
		char primary_drive_fs_name[11] = {0};
		strncpy(primary_drive_fs_name, MYOS_KERNEL_FILESYSTEM_NAME, strlen(MYOS_KERNEL_FILESYSTEM_NAME));
		disk->filesystem->volume_name(disk->fs_private, fs_name, sizeof(fs_name));
		if (strncmp(fs_name, primary_drive_fs_name, sizeof(fs_name)) == 0)
		{
			primary_fs_disk = disk;
		}
	}

	return 0;
}

long disk_real_sector(struct disk *idisk, unsigned int lba)
{
	size_t absolute_lba = idisk->starting_lba + lba;
	return absolute_lba;
}

long disk_real_offset(struct disk *idisk, unsigned int lba)
{
	size_t absolute_lba = disk_real_sector(idisk, lba);
	return absolute_lba * idisk->sector_size;
}

int disk_create_new(struct disk_driver *driver, struct disk *hardware_disk, int type, int starting_lba, int ending_lba, size_t sector_size, void *driver_private_data, struct disk **out_disk)
{
	int res = 0;
	struct disk *disk = kzalloc(sizeof(struct disk));
	if (!disk)
	{
		res = -ENOMEM;
		goto out;
	}

	if (hardware_disk && type == MYOS_DISK_TYPE_REAL)
	{
		res = -EINVARG;
		goto out;
	}

	if (type == MYOS_DISK_TYPE_REAL)
	{
		hardware_disk = disk;
	}

	if (!hardware_disk)
	{
		res = -EINVARG;
		goto out;
	}

	if (hardware_disk->type != MYOS_DISK_TYPE_REAL)
	{
		res = -EINVARG;
		goto out;
	}

	disk->type = type;
	disk->sector_size = sector_size;
	disk->starting_lba = starting_lba;
	disk->ending_lba = ending_lba;
	disk->driver = driver;
	disk->driver_private = driver_private_data;
	disk->hardware_disk = hardware_disk;
	disk->id = vector_count(disk_vector);
	disk->cache = diskstreamer_cache_new();

	if (out_disk)
	{
		*out_disk = disk;
	}

	res = vector_push(disk_vector, &disk);
	if (res < 0)
	{
		goto out;
	}

out:
	return res;
}

int disk_mount_all()
{
	int res = 0;
	res = disk_driver_mount_all();
	return res;
}

int disk_search_and_init()
{
	int res = 0;
	res = disk_driver_system_init();
	if (res < 0)
	{
		res = -EIO;
		goto out;
	}

	disk_vector = vector_new(sizeof(struct disk *), 4, 0);
	if (!disk_vector)
	{
		res = -ENOMEM;
		goto out;
	}

	res = disk_mount_all();
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
	vector_at(disk_vector, index, &disk, sizeof(disk));
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

	if (!idisk->driver->functions.read)
	{
		return -EIO;
	}

	return idisk->driver->functions.read(idisk, absolute_lba, total, buf);
}

void *disk_private_data_driver(struct disk *disk)
{
	return disk->driver_private;
}