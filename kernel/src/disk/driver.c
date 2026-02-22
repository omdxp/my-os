#include "driver.h"
#include "lib/vector.h"
#include "string/string.h"
#include "memory/memory.h"
#include "disk.h"
#include "drivers/pata.h"
#include "status.h"

struct vector *disk_driver_vec = NULL; // vector of all disk drivers in the system

int disk_driver_system_load_drivers()
{
	int res = 0;

	// register ATA/IDE pata
	res = disk_driver_register(pata_driver_init());
	if (res < 0)
	{
		goto out;
	}

	// TODO: register NVMe

out:
	return res;
}

int disk_driver_system_init()
{
	int res = 0;
	disk_driver_vec = vector_new(sizeof(struct disk_driver *), 8, 0);
	if (!disk_driver_vec)
	{
		res = -ENOMEM;
		goto out;
	}

	res = disk_driver_system_load_drivers();
	if (res < 0)
	{
		goto out;
	}

out:
	return res;
}

int disk_driver_mount_partition(struct disk_driver *driver, struct disk *disk, uint64_t starting_lba, uint64_t ending_lba, struct disk **partition_disk_out)
{
	int res = 0;
	if (driver != disk->driver || !disk->driver)
	{
		res = -EINVARG;
		goto out;
	}

	if (disk->type != MYOS_DISK_TYPE_REAL)
	{
		res = -EUNIMP;
		goto out;
	}

	res = disk->driver->functions.mount_partition(disk, starting_lba, ending_lba, partition_disk_out);

out:
	return res;
}

bool disk_driver_registered(struct disk_driver *driver)
{
	size_t total_drivers = vector_count(disk_driver_vec);
	for (size_t i = 0; i < total_drivers; i++)
	{
		struct disk_driver *d = NULL;
		vector_at(disk_driver_vec, i, &d, sizeof(d));
		if (d && strncmp(d->name, driver->name, sizeof(d->name)) == 0)
		{
			return true;
		}
	}

	return false;
}

void *disk_driver_private_data(struct disk_driver *driver)
{
	return driver->private;
}

int disk_driver_register(struct disk_driver *driver)
{
	int res = 0;
	if (disk_driver_registered(driver))
	{
		res = -EISTKN;
		goto out;
	}

	vector_push(disk_driver_vec, &driver);
	if (driver->functions.loaded)
	{
		res = driver->functions.loaded(driver);
		if (res < 0)
		{
			vector_pop_element(disk_driver_vec, &driver, sizeof(driver));
			goto out;
		}
	}

out:
	return res;
}

int disk_driver_mount_all()
{
	size_t total_drivers = vector_count(disk_driver_vec);
	for (size_t i = 0; i < total_drivers; i++)
	{
		struct disk_driver *driver = NULL;
		vector_at(disk_driver_vec, i, &driver, sizeof(driver));
		if (driver && driver->functions.mount)
		{
			driver->functions.mount(driver);
		}
	}

	return 0;
}