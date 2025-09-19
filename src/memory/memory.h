#pragma once

#include <stddef.h>
#include <stdint.h>

struct e820_entry
{
	uint64_t base_addr;			  // starting address
	uint64_t length;			  // length in bytes
	uint32_t type;				  // type of memory region
	uint32_t extended_attributes; // extended attributes
} __attribute__((packed));

struct e820_entry *e820_largest_free_entry();
size_t e820_total_accessible_memory();
size_t e820_total_entries();
struct e820_entry *e820_entry(size_t index);

void *memset(void *ptr, int c, size_t size);
int memcmp(void *s1, void *s2, int count);
void *memcpy(void *dest, void *src, int len);
