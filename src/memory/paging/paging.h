#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

enum
{
	PAGING_MAP_LEVEL_4 = 4,
	// level 5 not supported yet
};
typedef uint8_t paging_map_level_t;

#define PAGING_CACHE_DISABLED 0b00010000
#define PAGING_WRITE_THROUGH 0b00001000
#define PAGING_ACCESS_FROM_ALL 0b00000100
#define PAGING_IS_WRITEABLE 0b00000010
#define PAGING_IS_PRESENT 0b00000001

#define PAGING_TOTAL_ENTRIES_PER_TABLE 512

// 4 KB pages
#define PAGING_PAGE_SIZE 4096

enum
{
	PAGING_PLM4T_MAX_ADDRESSABLE = 0x80000000,		// 512 GB
	PAGING_PDPT_MAX_ADDRESSABLE = 0x40000000,		// 1 GB
	PAGING_PDT_MAX_ADDRESSABLE = 0x200000,			// 2 MB
	PAGING_PAGE_MAX_ADDRESSABLE = PAGING_PAGE_SIZE, // 4 KB
};

// page directory entry structure
struct paging_desc_entry
{
	uint64_t present : 1;		  // 0 = not present, 1 = present
	uint64_t read_write : 1;	  // 0 = read-only, 1 = read/write
	uint64_t user_supervisor : 1; // 0 = supervisor, 1 = user
	uint64_t pwt : 1;			  // page-level write-through
	uint64_t pcd : 1;			  // page-level cache disable
	uint64_t accessed : 1;		  // 0 = not accessed, 1 = accessed
	uint64_t ignored : 1;		  // ignored
	uint64_t reserved0 : 1;		  // must be 0
	uint64_t reserved1 : 4;		  // must be 0
	uint64_t address : 40;		  // physical address shifted right 12 bits
	uint64_t available : 11;	  // available for system programmer's use
	uint64_t execute_disable : 1; // 0 = executable, 1 = not executable
} __attribute__((packed));

// page map level 4 table structure
struct paging_pml_entries
{
	struct paging_desc_entry entries[PAGING_TOTAL_ENTRIES_PER_TABLE];
} __attribute__((packed));

// top-level paging structure
struct paging_desc
{
	struct paging_pml_entries *pml; // pointer to PML4 table
	paging_map_level_t level;		// current paging level
} __attribute__((packed));

int paging_map_to(struct paging_desc *desc, void *virt, void *phys, void *phys_end, int flags);
int paging_map_range(struct paging_desc *desc, void *virt, void *phys, size_t count, int flags);
int paging_map(struct paging_desc *desc, void *virt, void *phys, int flags);
void *paging_align_to_lower_page(void *addr);
void *paging_align_address(void *ptr);
struct paging_desc *paging_current_descriptor();
struct paging_desc *paging_desc_new(paging_map_level_t level);
bool paging_is_aligned(void *addr);
int paging_map_e820_memory_regions(struct paging_desc *desc);

void paging_load_directory(uintptr_t *directory);
void paging_invalidate_tlb_entry(uintptr_t addr);
void paging_switch(struct paging_desc *desc);

// struct paging_4gb_chunk
// {
// 	uint32_t *directory_entry;
// };

// struct paging_4gb_chunk *paging_new_4gb(uint8_t flags);
// void paging_switch(struct paging_4gb_chunk *directory);
// void enable_paging();

// int paging_set(uint32_t *directory, void *virt, uint32_t val);
// bool paging_is_aligned(void *addr);

// uint32_t *paging_4gb_chunk_get_directory(struct paging_4gb_chunk *chunk);
// void paging_free_4gb(struct paging_4gb_chunk *chunk);

// int paging_map_to(struct paging_4gb_chunk *directory, void *virt, void *phys, void *phys_end, int flags);
// int paging_map_range(struct paging_4gb_chunk *directory, void *virt, void *phys, int count, int flags);
// int paging_map(struct paging_4gb_chunk *directory, void *virt, void *phys, int flags);
// void *paging_align_address(void *ptr);
// void *paging_align_to_lower_page(void *addr);
// uint32_t paging_get(uint32_t *directory, void *virt);
// void *paging_get_phys(uint32_t *directory, void *virt);
