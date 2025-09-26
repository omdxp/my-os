#pragma once

#include <stdint.h>

struct gdt_entry
{
	uint16_t segment;
	uint16_t base_first;
	uint8_t base;
	uint8_t access;
	uint8_t high_flags;
	uint8_t base_24_31_bits;
} __attribute__((packed));

struct tss_desc_64
{
	uint16_t limit0;	  // limit bits 0-15
	uint16_t base0;		  // base bits 0-15
	uint8_t base1;		  // base bits 16-23
	uint8_t type;		  // type
	uint8_t limit1_flags; // limit bits 16-19 and flags
	uint8_t base2;		  // base bits 24-31
	uint32_t base3;		  // base bits 32-63
	uint32_t reserved;	  // reserved, set to 0

} __attribute__((packed));

void gdt_set(struct gdt_entry *gdt_entry, void *address, uint16_t limit_low, uint8_t access_byte, uint8_t flags);
void gdt_set_tss(struct tss_desc_64 *desc, void *tss_addr, uint16_t limit, uint8_t type, uint8_t flags);
