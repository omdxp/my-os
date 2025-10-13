#include "kheap.h"
#include "heap.h"
#include "config.h"
#include "kernel.h"
#include "memory/memory.h"
#include "memory/paging/paging.h"
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
		if (current->type == 1 && current->length > MYOS_HEAP_MINIMUM_SIZE_BYTES)
		{
			entry = current;
			break;
		}
	}
	return entry;
}

void kheap_post_paging()
{
	multiheap_ready(kernel_multiheap);
}

void *krealloc(void *old_ptr, size_t new_size)
{
	return multiheap_realloc(kernel_multiheap, old_ptr, new_size);
}

void kheap_init()
{
	struct e820_entry *entry = kheap_get_allowable_memory_region_for_minimal_heap();
	if (!entry)
	{
		panic("kheap_init: No suitable memory region found for the kernel heap\n");
	}

	void *address = (void *)entry->base_addr;
	void *end_address = (void *)(entry->base_addr + entry->length);
	void *heap_table_address = address;
	if (heap_table_address < (void *)MYOS_MINIMAL_HEAP_TABLE_ADDRESS)
	{
		heap_table_address = (void *)MYOS_MINIMAL_HEAP_TABLE_ADDRESS;
	}

	size_t total_heap_size = end_address - heap_table_address;
	size_t total_heap_blocks = total_heap_size / MYOS_HEAP_BLOCK_SIZE;
	size_t total_heap_entry_table_size = total_heap_blocks * sizeof(HEAP_BLOCK_TABLE_ENTRY);

	size_t heap_data_size = total_heap_size - total_heap_entry_table_size;

	size_t total_heap_data_blocks = heap_data_size / MYOS_HEAP_BLOCK_SIZE;
	total_heap_entry_table_size = total_heap_data_blocks * sizeof(HEAP_BLOCK_TABLE_ENTRY);

	void *heap_address = heap_table_address + total_heap_entry_table_size;
	void *heap_end_address = end_address;

	if (!paging_is_aligned(heap_address))
	{
		heap_address = paging_align_address(heap_address);
	}

	if (!paging_is_aligned(heap_end_address))
	{
		heap_end_address = paging_align_to_lower_page(heap_end_address);
	}

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
			void *base_addr = (void *)current->base_addr;
			void *end_addr = (void *)(current->base_addr + current->length);
			if (!paging_is_aligned(base_addr))
			{
				base_addr = paging_align_address(base_addr);
			}

			if (!paging_is_aligned(end_addr))
			{
				end_addr = paging_align_to_lower_page(end_addr);
			}

			if (base_addr < (void *)MYOS_MINIMAL_HEAP_ADDRESS)
			{
				base_addr = (void *)MYOS_MINIMAL_HEAP_ADDRESS;
			}

			if (end_addr <= base_addr)
			{
				continue;
			}

			multiheap_add(kernel_multiheap, (void *)base_addr, (void *)end_addr, MULTIHEAP_HEAP_FLAG_DEFRAGMENT_WITH_PAGING);
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
	void *ptr = kmalloc(size);
	if (!ptr)
	{
		return 0;
	}
	memset(ptr, 0x00, size);
	return ptr;
}

void *kpalloc(size_t size)
{
	void *ptr = multiheap_palloc(kernel_multiheap, size);
	if (!ptr)
	{
		panic("kpalloc: Out of memory\n");
	}
	return ptr;
}

void *kpzalloc(size_t size)
{
	void *ptr = kpalloc(size);
	if (!ptr)
	{
		return 0;
	}
	memset(ptr, 0x00, size);
	return ptr;
}

void kfree(void *ptr)
{
	// heap_free(&kernel_minimal_heap, ptr);
}
