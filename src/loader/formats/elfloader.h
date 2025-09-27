#pragma once

#include "elf.h"
#include "config.h"

#include <stdint.h>
#include <stddef.h>

struct elf_file
{
	char filename[MYOS_MAX_PATH];

	int in_memory_size;

	// physical mmeory address where this file is loaded at
	void *elf_memory;

	// virtual base address of this binary
	void *virtual_base_address;

	// ending virtual address
	void *virtual_end_address;

	// physical base address
	void *physical_base_address;

	// physical end address of this binary
	void *physical_end_address;
};

int elf_load(const char *filename, struct elf_file **file_out);
void elf_close(struct elf_file *file);
void *elf_virtual_base(struct elf_file *file);
void *elf_virtual_end(struct elf_file *file);
void *elf_phys_base(struct elf_file *file);
void *elf_phys_end(struct elf_file *file);
struct elf_header *elf_header(struct elf_file *file);
struct elf64_shdr *elf_sheader(struct elf_header *header);
struct elf64_phdr *elf_pheader(struct elf_header *header);
void *elf_memory(struct elf_file *file);
struct elf64_phdr *elf_program_header(struct elf_header *header, int index);
struct elf64_shdr *elf_section(struct elf_header *header, int index);
void *elf_phdr_phys_address(struct elf_file *file, struct elf64_phdr *phdr);
