#pragma once

#include <stdint.h>
#include <stddef.h>

#define PF_X 0x01
#define PF_W 0x02
#define PF_R 0x04

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMBTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_SHLIB 10
#define SHT_DYNSYM 11
#define SHT_LOPROC 12
#define SHT_HIPROC 13
#define SHT_LOUSER 14
#define SHT_HIUSER 15

#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4

#define EI_NIDENT 16
#define EI_CLASS 4
#define EI_DATA 5

#define ELFCLASSNONE 0
#define ELFCLASS32 1
#define ELFCLASS64 2

#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define SHN_UNDEF 0

typedef uint64_t elf64_addr;
typedef uint64_t elf64_off;
typedef uint16_t elf64_half;
typedef uint32_t elf64_word;
typedef uint32_t elf64_sword;
typedef uint64_t elf64_xword;
typedef uint64_t elf64_sxword;

struct elf64_phdr
{
	elf64_word p_type;
	elf64_word p_flags;
	elf64_off p_offset;
	elf64_addr p_vaddr;
	elf64_addr p_paddr;
	elf64_xword p_filesz;
	elf64_xword p_memsz;
	elf64_xword p_align;
} __attribute__((packed));

struct elf64_shdr
{
	elf64_word sh_name;
	elf64_xword sh_type;
	elf64_xword sh_flags;
	elf64_addr sh_addr;
	elf64_off sh_offset;
	elf64_xword sh_size;
	elf64_word sh_link;
	elf64_word sh_info;
	elf64_xword sh_addralign;
	elf64_xword sh_entsize;
} __attribute__((packed));

struct elf_header
{
	unsigned char e_ident[EI_NIDENT];
	elf64_half e_type;
	elf64_half e_machine;
	elf64_word e_version;
	elf64_addr e_entry;
	elf64_off e_phoff;
	elf64_off e_shoff;
	elf64_word e_flags;
	elf64_half e_ehsize;
	elf64_half e_phentsize;
	elf64_half e_phnum;
	elf64_half e_shentsize;
	elf64_half e_shnum;
	elf64_half e_shstrndx;
} __attribute__((packed));

struct elf64_dyn
{
	elf64_sxword d_tag;
	union
	{
		elf64_xword d_val;
		elf64_addr d_ptr;
	} d_un;
} __attribute__((packed));

struct elf64_sym
{
	elf64_word st_name;
	unsigned char st_info;
	unsigned char st_other;
	elf64_half st_shndx;
	elf64_addr st_value;
	elf64_xword st_size;
} __attribute__((packed));

void *elf_get_entry_ptr(struct elf_header *elf_header);
elf64_addr elf_get_entry(struct elf_header *elf_header);
