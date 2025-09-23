#include "multiheap.h"
#include "kernel.h"
#include "memory/paging/paging.h"
#include "status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct multiheap *multiheap_new(struct heap *starting_heap)
{
	if (!starting_heap)
	{
		return 0;
	}

	struct multiheap *mh = heap_zalloc(starting_heap, sizeof(struct multiheap));
	if (!mh)
	{
		goto out;
	}
	mh->starting_heap = starting_heap;
	mh->first_multiheap = 0;
	mh->total_heaps = 0;
out:
	return mh;
}

struct multiheap_single_heap *multiheap_get_last_heap(struct multiheap *mh)
{
	if (!mh)
	{
		return 0;
	}
	struct multiheap_single_heap *current = mh->first_multiheap;
	if (!current)
	{
		return 0;
	}
	while (current->next)
	{
		current = current->next;
	}
	return current;
}

static bool multiheap_heap_allows_paging(struct multiheap_single_heap *mhs)
{
	return mhs->flags & MULTIHEAP_HEAP_FLAG_DEFRAGMENT_WITH_PAGING;
}

void *multiheap_get_max_memory_end_address(struct multiheap *mh)
{
	void *max_addr = 0x00;
	struct multiheap_single_heap *current = mh->first_multiheap;
	while (current)
	{
		if (current->heap->eaddr >= max_addr)
		{
			max_addr = current->heap->eaddr;
		}
		current = current->next;
	}
	return max_addr;
}

struct multiheap_single_heap *multiheap_get_heap_for_address(struct multiheap *mh, void *addr)
{
	struct multiheap_single_heap *current = mh->first_multiheap;
	while (current)
	{
		if (heap_is_address_within_heap(current->heap, addr))
		{
			return current;
		}
		current = current->next;
	}
	return NULL;
}

bool multiheap_is_address_virtual(struct multiheap *mh, void *addr)
{
	return addr >= mh->max_end_data_addr;
}

bool multiheap_is_ready(struct multiheap *mh)
{
	return mh && (mh->flags & MULTIHEAP_FLAG_IS_READY);
}

bool multiheap_can_add_heap(struct multiheap *mh)
{
	return !multiheap_is_ready(mh);
}

void *multiheap_virtual_address_to_physical(struct multiheap *mh, void *addr)
{
	void *phys_addr = (void *)((uintptr_t)addr - (uintptr_t)mh->max_end_data_addr);
	return phys_addr;
}

struct multiheap_single_heap *multiheap_get_paging_heap_for_address(struct multiheap *mh, void *addr)
{
	struct multiheap_single_heap *current = mh->first_multiheap;
	while (current)
	{
		if (!multiheap_heap_allows_paging(current))
		{
			current = current->next;
			continue;
		}

		if (heap_is_address_within_heap(current->paging_heap, addr))
		{
			return current;
		}

		current = current->next;
	}

	return NULL;
}

void multiheap_get_heap_and_paging_heap_for_address(struct multiheap *mh, void *ptr, struct multiheap_single_heap **out_heap, struct multiheap_single_heap **out_paging_heap, void **real_phys_addr)
{
	void *real_addr = ptr;
	if (multiheap_is_address_virtual(mh, ptr))
	{
		*out_paging_heap = multiheap_get_paging_heap_for_address(mh, ptr);
		real_addr = multiheap_virtual_address_to_physical(mh, ptr);
	}

	*out_heap = multiheap_get_heap_for_address(mh, real_addr);
	*real_phys_addr = real_addr;
}

size_t multiheap_allocation_block_count(struct multiheap *mh, void *ptr)
{
	struct multiheap_single_heap *paging_heap = NULL;
	struct multiheap_single_heap *phys_heap = NULL;
	struct multiheap_single_heap *heap_to_check = NULL;
	void *real_addr = NULL;

	multiheap_get_heap_and_paging_heap_for_address(mh, ptr, &phys_heap, &paging_heap, &real_addr);

	if (paging_heap)
	{
		heap_to_check = paging_heap;
	}

	if (!heap_to_check) // not allocated from us
	{
		return 0;
	}

	size_t total_blocks = heap_allocation_block_count(heap_to_check->heap, real_addr);
	return total_blocks;
}

size_t multiheap_allocation_byte_count(struct multiheap *mh, void *ptr)
{
	return multiheap_allocation_block_count(mh, ptr) * MYOS_HEAP_BLOCK_SIZE;
}

int multiheap_add_heap(struct multiheap *mh, struct heap *heap, int flags)
{
	if (!mh || !heap || !multiheap_can_add_heap(mh))
	{
		return -EINVARG;
	}
	struct multiheap_single_heap *mhs = heap_zalloc(mh->starting_heap, sizeof(struct multiheap_single_heap));
	if (!mhs)
	{
		return -ENOMEM;
	}
	mhs->heap = heap;
	mhs->flags = flags;
	mhs->next = 0;
	if (!mh->first_multiheap)
	{
		mh->first_multiheap = mhs;
	}
	else
	{
		struct multiheap_single_heap *last = multiheap_get_last_heap(mh);
		last->next = mhs;
	}
	mh->total_heaps++;
	return 0;
}

int multiheap_add_existing_heap(struct multiheap *mh, struct heap *heap, int flags)
{
	if (!mh || !heap)
	{
		return -EINVARG;
	}
	return multiheap_add_heap(mh, heap, flags | MULTIHEAP_HEAP_FLAG_EXTERNALLY_OWNED);
}

int multiheap_add(struct multiheap *mh, void *saddr, void *eaddr, int flags)
{
	struct heap *heap = heap_zalloc(mh->starting_heap, sizeof(struct heap));
	struct heap_table *table = heap_zalloc(mh->starting_heap, sizeof(struct heap_table));
	if (!heap || !table)
	{
		return -ENOMEM;
	}
	int res = heap_create(heap, saddr, eaddr, table);
	if (res < 0)
	{
		heap_free(mh->starting_heap, heap);
		heap_free(mh->starting_heap, table);
		return res;
	}
	return multiheap_add_heap(mh, heap, flags);
}

void multiheap_free(struct multiheap *mh, void *ptr)
{
	struct multiheap_single_heap *paging_heap = NULL;
	struct multiheap_single_heap *phys_heap = NULL;
	void *real_phys_addr = NULL;

	multiheap_get_heap_and_paging_heap_for_address(mh, ptr, &phys_heap, &paging_heap, &real_phys_addr);

	if (paging_heap)
	{
		size_t total_blocks = heap_allocation_block_count(paging_heap->paging_heap, ptr);
		size_t starting_block = heap_address_to_block(paging_heap->paging_heap, ptr);
		size_t ending_block = starting_block + total_blocks;
		for (size_t i = starting_block; i < ending_block; i++)
		{
			void *virt_addr_for_block = (void *)((uintptr_t)ptr) + (i * MYOS_HEAP_BLOCK_SIZE);
			void *data_phys_addr = paging_get_physical_address(paging_current_descriptor(), virt_addr_for_block);

			multiheap_free(mh, data_phys_addr);
		}

		heap_free(paging_heap->paging_heap, ptr);
	}
	else if (phys_heap)
	{
		heap_free(phys_heap->heap, real_phys_addr);
	}
}

void multiheap_free_heap(struct multiheap *mh)
{
	if (!mh)
	{
		return;
	}
	struct multiheap_single_heap *current = mh->first_multiheap;
	while (current)
	{
		struct multiheap_single_heap *next = current->next;
		if (!(current->flags & MULTIHEAP_HEAP_FLAG_EXTERNALLY_OWNED))
		{
			heap_free(mh->starting_heap, current->heap);
		}
		current = next;
	}
	heap_free(mh->starting_heap, mh);
}

void *multiheap_alloc_first_pass(struct multiheap *mh, size_t size)
{
	if (!mh || size == 0)
	{
		return 0;
	}
	struct multiheap_single_heap *current = mh->first_multiheap;
	while (current)
	{
		void *ptr = heap_malloc(current->heap, size);
		if (ptr)
		{
			return ptr;
		}
		current = current->next;
	}
	return 0;
}

void *multiheap_alloc_paging(struct multiheap *mh, size_t size, struct multiheap_single_heap **out_eligible_heap)
{
	void *allocation_ptr = NULL;
	size_t total_required_blocks = size / MYOS_HEAP_BLOCK_SIZE;
	struct multiheap_single_heap *current = mh->first_multiheap;
	while (current)
	{
		if (!multiheap_heap_allows_paging(current))
		{
			current = current->next;
			continue;
		}

		if (current->heap->free_blocks < total_required_blocks)
		{
			current = current->next;
			continue;
		}

		allocation_ptr = heap_malloc(current->paging_heap, size);
		if (allocation_ptr)
		{
			if (out_eligible_heap)
			{
				*out_eligible_heap = current;
			}
			break;
		}

		current = current->next;
	}

	return allocation_ptr;
}

void *multiheap_alloc_second_pass(struct multiheap *mh, size_t size)
{
	void *allocation_ptr = NULL;
	struct paging_desc *paging_desc = paging_current_descriptor();
	if (!paging_desc)
	{
		panic("multiheap_alloc_second_pass: paging_current_descriptor failed\n");
	}

	size = heap_align_value_to_upper(size);
	size_t total_blocks = size / MYOS_HEAP_BLOCK_SIZE;
	struct multiheap_single_heap *chosen_real_heap = NULL;

	void *defragmented_virtual_memory_saddr = multiheap_alloc_paging(mh, size, &chosen_real_heap);
	if (!defragmented_virtual_memory_saddr)
	{
		allocation_ptr = NULL;
		goto out;
	}

	void *defragmented_virtual_memory_current_addr = defragmented_virtual_memory_saddr;
	allocation_ptr = defragmented_virtual_memory_saddr;

	for (size_t i = 0; i < total_blocks; i++)
	{
		void *block_addr = heap_zalloc(chosen_real_heap->heap, MYOS_HEAP_BLOCK_SIZE);
		if (!block_addr)
		{
			panic("multiheap_alloc_second_pass: heap_zalloc failed\n");
		}

		paging_map(paging_desc, defragmented_virtual_memory_current_addr, block_addr, PAGING_IS_PRESENT | PAGING_IS_WRITEABLE);
		defragmented_virtual_memory_current_addr += (uint64_t)MYOS_HEAP_BLOCK_SIZE;
	}

out:
	return allocation_ptr;
}

void multiheap_paging_heap_free_block(void *ptr)
{
	paging_map(paging_current_descriptor(), ptr, NULL, 0);
}

int multiheap_ready(struct multiheap *mh)
{
	int res = 0;
	mh->flags |= MULTIHEAP_FLAG_IS_READY;

	struct paging_desc *paging_desc = paging_current_descriptor();
	if (!paging_desc)
	{
		panic("multiheap_ready: paging_current_descriptor failed\n");
	}

	void *max_end_addr = multiheap_get_max_memory_end_address(mh);
	mh->max_end_data_addr = max_end_addr;

	struct multiheap_single_heap *current = mh->first_multiheap;
	while (current)
	{
		if (multiheap_heap_allows_paging(current))
		{
			void *paging_heap_starting_address = max_end_addr + (uint64_t)current->heap->saddr;
			void *paging_heap_ending_address = max_end_addr + (uint64_t)current->heap->eaddr;

			struct heap_table *paging_heap_table = heap_zalloc(mh->starting_heap, sizeof(struct heap_table));
			paging_heap_table->entries = heap_zalloc(mh->starting_heap, sizeof(HEAP_BLOCK_TABLE_ENTRY) * current->heap->table->total);
			paging_heap_table->total = current->heap->table->total;

			struct heap *paging_heap = heap_zalloc(mh->starting_heap, sizeof(struct heap));
			heap_create(paging_heap, paging_heap_starting_address, paging_heap_ending_address, paging_heap_table);

			paging_map_to(paging_current_descriptor(), paging_heap_starting_address, paging_heap_starting_address, paging_heap_ending_address, 0);

			heap_callbacks_set(paging_heap, NULL, multiheap_paging_heap_free_block);
			current->paging_heap = paging_heap;
		}

	out:
		current = current->next;
	}
	return res;
}

void *multiheap_alloc(struct multiheap *mh, size_t size)
{
	void *allocation_ptr = multiheap_alloc_first_pass(mh, size);
	if (allocation_ptr)
	{
		return allocation_ptr;
	}
	// normal alloc does not defragment with paging
	return 0;
}

void *multiheap_palloc(struct multiheap *mh, size_t size)
{
	void *allocation_ptr = multiheap_alloc_first_pass(mh, size);
	if (allocation_ptr)
	{
		return allocation_ptr;
	}
	allocation_ptr = multiheap_alloc_second_pass(mh, size);
	return allocation_ptr;
}
