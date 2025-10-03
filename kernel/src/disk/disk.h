#pragma once

#include "fs/file.h"
#include <stddef.h>

typedef unsigned int MYOS_DISK_TYPE;

// represents real physical hard disk
#define MYOS_DISK_TYPE_REAL 0

// specifies this disk is a partition/virtual disk
#define MTOS_DISK_TYPE_PARTITION 1

// represents the kernel filesystem name
#define MYOS_KERNEL_FILESYSTEM_NAME "MYOSFS     "

struct disk
{
	MYOS_DISK_TYPE type;
	int sector_size;

	// disk ID
	int id;

	struct filesystem *filesystem;

	// set both to zero for primary disk
	// all bounds checks are ignored if starting_lba and ending_lba are zero
	size_t starting_lba; // starting LBA of this disk (for partitions)
	size_t ending_lba;	 // ending LBA of this disk (for partitions)

	// private data of our filesystem
	void *fs_private;
};

int disk_search_and_init();
struct disk *disk_get(int index);
int disk_read_block(struct disk *idisk, unsigned int lba, int total, void *buf);
int disk_create_new(int type, int starting_lba, int ending_lba, size_t sector_size, struct disk **out_disk);
