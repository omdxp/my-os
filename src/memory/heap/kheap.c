#include "kheap.h"
#include "heap.h"
#include "config.h"
#include "kernel.h"
#include "memory/memory.h"
#include "multiheap.h"

struct heap kernel_minimal_heap;
struct heap_table kernel_minimal_heap_table;

struct multiheap *kernel_multiheap = NULL;

struct e820_entry *kheap_get_allowable_memory_region_for_minimal_heap()
{
	struct e820_entry *entry = 0;
	size_t total_entries = e820_total_entries();
	for (size_t i = 0; i < total_entries; i++)
	{
		struct e820_entry *current = e820_entry(i);
		if (current->type == 1 && current->length > MYOS_HEAP_SIZE_BYTES)
		{
			entry = current;
			break;
		}
	}
	return entry;
}

void kheap_init()
{
	struct e820_entry *entry = kheap_get_allowable_memory_region_for_minimal_heap();
	if (!entry)
	{
		panic("kheap_init: No suitable memory region found for the kernel heap\n");
	}

	void *address = (void *)entry->base_addr;
	void *heap_table_address = address;
	if (heap_table_address < (void *)MYOS_MINIMAL_HEAP_TABLE_ADDRESS)
	{
		heap_table_address = (void *)MYOS_MINIMAL_HEAP_TABLE_ADDRESS;
	}

	void *heap_address = heap_table_address + MYOS_MINIMAL_HEAP_TABLE_SIZE;
	void *heap_end_address = heap_address + MYOS_HEAP_SIZE_BYTES;
	size_t size = heap_end_address - heap_address;
	size_t total_table_entries = size / MYOS_HEAP_BLOCK_SIZE;
	kernel_minimal_heap_table.entries = (HEAP_BLOCK_TABLE_ENTRY *)heap_table_address;
	kernel_minimal_heap_table.total = total_table_entries;

	int res = heap_create(&kernel_minimal_heap, heap_address, heap_end_address, &kernel_minimal_heap_table);
	if (res < 0)
	{
		panic("kheap_init: heap_create failed\n");
	}

	kernel_multiheap = multiheap_new(&kernel_minimal_heap);
	if (!kernel_multiheap)
	{
		panic("kheap_init: multiheap_new failed\n");
	}
	res = multiheap_add_existing_heap(kernel_multiheap, &kernel_minimal_heap, MULTIHEAP_HEAP_FLAG_EXTERNALLY_OWNED);
	if (res < 0)
	{
		panic("kheap_init: multiheap_add_existing_heap failed\n");
	}

	struct e820_entry *used_entru = entry;

	size_t total_entries = e820_total_entries();
	for (size_t i = 0; i < total_entries; i++)
	{
		struct e820_entry *current = e820_entry(i);
		if (current->type == 1 && current != used_entru)
		{
			multiheap_add(kernel_multiheap, (void *)current->base_addr, (void *)(current->base_addr + current->length), MULTIHEAP_HEAP_FLAG_DEFRAGMENT_WITH_PAGING);
		}
	}
}

void *kmalloc(size_t size)
{
	void *ptr = multiheap_alloc(kernel_multiheap, size);
	if (!ptr)
	{
		return 0;
	}
	return ptr;
}

void *kzalloc(size_t size)
{
	return NULL;
	// void *ptr = kmalloc(size);
	// if (!ptr)
	// {
	// 	return 0;
	// }
	// memset(ptr, 0x00, size);
	// return ptr;
}

void kfree(void *ptr)
{
	// heap_free(&kernel_minimal_heap, ptr);
}
