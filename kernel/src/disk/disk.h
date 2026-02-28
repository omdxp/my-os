#pragma once

#include "fs/file.h"
#include <stddef.h>
#include <stdint.h>
#include <stddef.h>

typedef unsigned int MYOS_DISK_TYPE;

// represents real physical hard disk
#define MYOS_DISK_TYPE_REAL 0

// specifies this disk is a partition/virtual disk
#define MYOS_DISK_TYPE_PARTITION 1

// represents the kernel filesystem name
#define MYOS_KERNEL_FILESYSTEM_NAME "MYOSFS     "

struct disk_driver;
struct disk_stream_cache;
struct disk
{
	MYOS_DISK_TYPE type;
	int sector_size;

	// disk ID
	int id;

	struct filesystem *filesystem;

	struct disk_driver *driver; // the disk driver responsible for this disk
	struct disk *hardware_disk; // the hardware disk this disk is attached to

	struct disk_stream_cache *cache; // cache for disk streaming

	// set both to zero for primary disk
	// all bounds checks are ignored if starting_lba and ending_lba are zero
	size_t starting_lba; // starting LBA of this disk (for partitions)
	size_t ending_lba;	 // ending LBA of this disk (for partitions)

	// private data of our filesystem
	void *fs_private;

	// private data of the disk driver
	void *driver_private;
};

int disk_search_and_init();
struct disk *disk_get(int index);
int disk_read_block(struct disk *idisk, unsigned int lba, int total, void *buf);
int disk_create_new(struct disk_driver *driver, struct disk *hardware_disk, int type, int starting_lba, int ending_lba, size_t sector_size, void *driver_private_data, struct disk **out_disk);
struct disk *disk_primary_fs_disk();
struct disk *disk_primary();
struct disk *disk_hardware_disk(struct disk *disk);
int disk_filesystem_mount(struct disk *disk);
int disk_create_partition(struct disk *disk, uint64_t starting_lba, uint64_t ending_lba, struct disk **partition_disk_out);
void *disk_private_data_driver(struct disk *disk);
long disk_real_sector(struct disk *idisk, unsigned int lba);
long disk_real_offset(struct disk *idisk, unsigned int lba);