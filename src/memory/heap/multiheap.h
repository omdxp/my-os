#pragma once

#include "heap.h"

enum
{
	MULTIHEAP_HEAP_FLAG_EXTERNALLY_OWNED = 0x01,	   // heap memory is owned by external entity and should not be freed
	MULTIHEAP_HEAP_FLAG_DEFRAGMENT_WITH_PAGING = 0x02, // defragmentation can use paging to move pages around
};

struct multiheap_single_heap
{
	struct heap *heap;
	struct heap *paging_heap; // if defragmentation with paging is enabled, this is the paging heap
	int flags;
	struct multiheap_single_heap *next;
};

enum
{
	MULTIHEAP_FLAG_IS_READY = 0x01, // multiheap is ready to use
};

struct multiheap
{
	struct heap *starting_heap;					   // used to allocate soace for multiheap
	struct multiheap_single_heap *first_multiheap; // linked list of heaps
	size_t total_heaps;							   // total number of heaps
	int flags;									   // multiheap flags
	void *max_end_data_addr;					   // maximum end address of all heaps
};

int multiheap_add_existing_heap(struct multiheap *mh, struct heap *heap, int flags);
int multiheap_add(struct multiheap *mh, void *saddr, void *eaddr, int flags);
void *multiheap_alloc(struct multiheap *mh, size_t size);
void *multiheap_palloc(struct multiheap *mh, size_t size);
struct multiheap *multiheap_new(struct heap *starting_heap);
void multiheap_free_heap(struct multiheap *mh);
void multiheap_free(struct multiheap *mh, void *ptr);
