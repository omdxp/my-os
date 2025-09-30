#include "gdt.h"
#include "kernel.h"
#include "memory/memory.h"

void gdt_set(struct gdt_entry *gdt_entry, void *address, uint16_t limit_low, uint8_t access_byte, uint8_t flags)
{
	if ((uintptr_t)address > 0xFFFFFFFF)
	{
		panic("gdt_set: Address out of range");
	}

	uint32_t base = (uint32_t)(uintptr_t)address;
	gdt_entry->base_first = base & 0xFFFF;			  // lower 16 bits of base
	gdt_entry->base = (base >> 16) & 0xFF;			  // next 8 bits of base
	gdt_entry->base_24_31_bits = (base >> 24) & 0xFF; // last 8 bits of base
	gdt_entry->segment = limit_low & 0xFFFF;		  // lower 16 bits of limit
	gdt_entry->access = access_byte;				  // access byte
	gdt_entry->high_flags = flags;					  // flags and upper 4 bits of limit
}

void gdt_set_tss(struct tss_desc_64 *desc, void *tss_addr, uint16_t limit, uint8_t type, uint8_t flags)
{
	memset(desc, 0, sizeof(struct tss_desc_64));
	desc->limit0 = limit & 0xFFFF;						  // limit bits 0-15
	desc->limit1_flags = (uint8_t)((limit >> 16) & 0x0F); // limit bits 16-19

	uint64_t base = (uint64_t)(uintptr_t)tss_addr;
	desc->base0 = (uint16_t)(base & 0xFFFF);			 // base bits 0-15
	desc->base1 = (uint8_t)((base >> 16) & 0xFF);		 // base bits 16-23
	desc->base2 = (uint8_t)((base >> 24) & 0xFF);		 // base bits 24-31
	desc->base3 = (uint32_t)((base >> 32) & 0xFFFFFFFF); // base bits 32-63

	desc->type = type;	// type (should be 0x89 for 64-bit TSS)
	desc->reserved = 0; // reserved, set to 0
}
