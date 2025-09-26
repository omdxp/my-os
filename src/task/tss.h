#pragma once

#include <stdint.h>

struct tss
{
	uint16_t link;		  // previous task link (16 bits)
	uint16_t reserved0;	  // reserved (16 bits)
	uint64_t rsp0;		  // stack pointer for ring 0 (64 bits)
	uint64_t rsp1;		  // stack pointer for ring 1 (64 bits)
	uint64_t rsp2;		  // stack pointer for ring 2 (64 bits)
	uint64_t reserved1;	  // reserved (64 bits)
	uint64_t ist1;		  // interrupt stack table 1 (64 bits)
	uint64_t ist2;		  // interrupt stack table 2 (64 bits)
	uint64_t ist3;		  // interrupt stack table 3 (64 bits)
	uint64_t ist4;		  // interrupt stack table 4 (64 bits)
	uint64_t ist5;		  // interrupt stack table 5 (64 bits)
	uint64_t ist6;		  // interrupt stack table 6 (64 bits)
	uint64_t ist7;		  // interrupt stack table 7 (64 bits)
	uint64_t reserved2;	  // reserved (64 bits)
	uint16_t reserved3;	  // reserved (16 bits)
	uint16_t iopb_offset; // I/O map base address (16 bits)
} __attribute__((packed));

void tss_load(int tss_segment);
