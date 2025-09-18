#include "memory.h"
#include "config.h"

struct e820_entry *e820_largest_free_entry()
{
	size_t total_memory_entries = *(uint16_t *)MYOS_MEMORY_MAP_TOTAL_ENTRIES_LOCATION;
	struct e820_entry *entries = (struct e820_entry *)MYOS_MEMORY_MAP_LOCATION;

	// care only about long contiguous free memory regions
	struct e820_entry *chosen_entry = NULL;
	for (size_t i = 0; i < total_memory_entries; i++)
	{
		struct e820_entry *entry = &entries[i];
		if (entry->type == 1) // type 1 is free RAM
		{
			if (chosen_entry == NULL)
			{
				chosen_entry = entry;
				continue;
			}

			if (entry->length > chosen_entry->length)
			{
				chosen_entry = entry;
			}
		}
	}
	return chosen_entry;
}

size_t e820_total_accessible_memory()
{
	size_t total_memory_entries = *(uint16_t *)MYOS_MEMORY_MAP_TOTAL_ENTRIES_LOCATION;
	struct e820_entry *entries = (struct e820_entry *)MYOS_MEMORY_MAP_LOCATION;

	size_t total_accessible_memory = 0;
	for (size_t i = 0; i < total_memory_entries; i++)
	{
		struct e820_entry *entry = &entries[i];
		if (entry->type == 1) // type 1 is free RAM
		{
			total_accessible_memory += entry->length;
		}
	}
	return total_accessible_memory;
}

void *memset(void *ptr, int c, size_t size)
{
	char *c_ptr = (char *)ptr;
	for (size_t i = 0; i < size; i++)
	{
		c_ptr[i] = (char)c;
	}

	return ptr;
}

int memcmp(void *s1, void *s2, int count)
{
	char *c1 = s1;
	char *c2 = s2;
	while (count-- > 0)
	{
		if (*c1++ != *c2++)
		{
			return c1[-1] < c2[-1] ? -1 : 1;
		}
	}

	return 0;
}

void *memcpy(void *dest, void *src, int len)
{
	char *d = dest;
	char *s = src;
	while (len--)
	{
		*d++ = *s++;
	}
	return dest;
}
