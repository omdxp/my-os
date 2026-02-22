#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DISK_DRIVER_NAME_SIZE 20

struct disk;
struct disk_driver;

typedef int (*DISK_DRIVER_LOADED)(struct disk_driver *driver);
typedef void (*DISK_DRIVER_UNLOADED)(struct disk_driver *driver);

typedef int (*DISK_DRIVER_MOUNT)(struct disk_driver *driver);
typedef void (*DISK_DRIVER_UNMOUNT)(struct disk *disk);

typedef int (*DISK_DRIVER_WRITE)(struct disk *disk, uint64_t lba, uint32_t total_sectors, const void *buf);
typedef int (*DISK_DRIVER_READ)(struct disk *disk, uint64_t lba, uint32_t total_sectors, void *buf);
typedef int (*DISK_DRIVER_MOUNT_PARTITION)(struct disk *disk, uint64_t starting_lba, uint64_t ending_lba, struct disk **partition_disk_out);

struct disk_driver
{
	char name[DISK_DRIVER_NAME_SIZE]; // name of this disk driver

	struct
	{
		DISK_DRIVER_LOADED loaded;					 // called when the driver is loaded, used for initialization
		DISK_DRIVER_UNLOADED unloaded;				 // called when the driver is unloaded, used for cleanup
		DISK_DRIVER_MOUNT mount;					 // called when a disk is mounted, used for initialization
		DISK_DRIVER_UNMOUNT unmount;				 // called when a disk is unmounted, used for cleanup
		DISK_DRIVER_READ read;						 // called to read from a disk
		DISK_DRIVER_WRITE write;					 // called to write to a disk
		DISK_DRIVER_MOUNT_PARTITION mount_partition; // called to mount a partition on a hardware disk, used for partition discovery and mounting
	} functions;

	void *private; // private data of the disk driver, used by the disk driver
};

int disk_driver_mount_all();
int disk_driver_register(struct disk_driver *driver);
bool disk_driver_registered(struct disk_driver *driver);
void *disk_driver_private_data(struct disk_driver *driver);
int disk_driver_mount_partition(struct disk_driver *driver, struct disk *disk, uint64_t starting_lba, uint64_t ending_lba, struct disk **partition_disk_out);
int disk_driver_system_init();
int disk_driver_system_load_drivers();