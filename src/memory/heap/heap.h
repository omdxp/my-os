#pragma once

#include "config.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define HEAP_BLOCK_TABLE_ENTRY_TAKEN 0x01
#define HEAP_BLOCK_TABLE_ENTRY_FREE 0x00

#define HEAP_BLOCK_HAS_NEXT 0b10000000
#define HEAP_BLOCK_IS_FIRST 0b01000000

typedef unsigned char HEAP_BLOCK_TABLE_ENTRY;

typedef void *(*HEAP_BLOCK_ALLOCATED_CALBACK_FUNCTION)(void *ptr, size_t size);
typedef void (*HEAP_BLOCK_FREED_CALLBACK_FUNCTION)(void *ptr);

struct heap_table
{
	HEAP_BLOCK_TABLE_ENTRY *entries;
	size_t total;
};

struct heap
{
	struct heap_table *table;										// pointer to the heap block table
	void *saddr;													// start address of the heap data pool
	void *eaddr;													// end address of the heap data pool
	HEAP_BLOCK_ALLOCATED_CALBACK_FUNCTION block_allocated_callback; // called when a block is allocated
	HEAP_BLOCK_FREED_CALLBACK_FUNCTION block_freed_callback;		// called when a block is freed
};

void heap_callbacks_set(struct heap *heap, HEAP_BLOCK_ALLOCATED_CALBACK_FUNCTION allocated_callback, HEAP_BLOCK_FREED_CALLBACK_FUNCTION freed_callback);

int heap_create(struct heap *heap, void *ptr, void *end, struct heap_table *table);
void *heap_malloc(struct heap *heap, size_t size);
void heap_free(struct heap *heap, void *ptr);
void *heap_zalloc(struct heap *heap, size_t size);
void *heap_kalloc(struct heap *heap, size_t size);

size_t heap_total_size(struct heap *heap);
size_t heap_total_used(struct heap *heap);
size_t heap_total_available(struct heap *heap);
bool heap_is_address_within_heap(struct heap *heap, void *addr);

uintptr_t heap_align_value_to_upper(uintptr_t val);
uintptr_t heap_align_value_to_lower(uintptr_t val);
