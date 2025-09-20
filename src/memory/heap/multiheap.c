#include "multiheap.h"
#include "kernel.h"
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

void *multiheap_virtual_address_to_physical(struct multiheap *mh, void *addr)
{
	void *phys_addr = (void *)((uintptr_t)addr - (uintptr_t)mh->max_end_data_addr);
	return phys_addr;
}

int multiheap_add_heap(struct multiheap *mh, struct heap *heap, int flags)
{
	if (!mh || !heap)
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

void multiheap_free(struct multiheap *mh)
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

void *multiheap_alloc_second_pass(struct multiheap *mh, size_t size)
{
	// TODO: implement defragmentation and allocation with paging here
	return 0;
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
