#include "paging.h"
#include "memory/heap/kheap.h"
#include "memory/memory.h"
#include "memory/heap/heap.h"
#include "status.h"

static struct paging_desc *current_paging_desc = NULL;

struct paging_pml_entries *paging_pml4_entries_new()
{
	struct paging_pml_entries *pml4 = kzalloc(sizeof(struct paging_pml_entries));
	if (!pml4)
	{
		return NULL;
	}

	return pml4;
}

static bool paging_map_level_is_valid(paging_map_level_t level)
{
	return level == PAGING_MAP_LEVEL_4;
}

void *paging_align_address(void *ptr)
{
	if ((uintptr_t)ptr % PAGING_PAGE_SIZE)
	{
		return (void *)((uintptr_t)ptr + PAGING_PAGE_SIZE - ((uintptr_t)ptr % PAGING_PAGE_SIZE));
	}

	return ptr;
}

void *paging_align_to_lower_page(void *addr)
{
	uintptr_t _addr = (uintptr_t)addr;
	_addr -= _addr % PAGING_PAGE_SIZE;
	return (void *)_addr;
}

void paging_switch(struct paging_desc *desc)
{
	current_paging_desc = desc;
	paging_load_directory((uint64_t *)(&desc->pml->entries[0]));
}

struct paging_desc *paging_desc_new(paging_map_level_t level)
{
	if (!paging_map_level_is_valid(level))
	{
		return NULL;
	}

	struct paging_desc *desc = kzalloc(sizeof(struct paging_desc));
	if (!desc)
	{
		return NULL;
	}

	desc->pml = paging_pml4_entries_new();
	if (!desc->pml)
	{
		kfree(desc);
		return NULL;
	}

	desc->level = level;
	return desc;
}

bool paging_is_aligned(void *addr)
{
	return ((uintptr_t)addr % PAGING_PAGE_SIZE) == 0;
}

static bool paging_null_entry(struct paging_desc_entry *entry)
{
	struct paging_desc_entry null_entry = {0};
	return memcmp(entry, &null_entry, sizeof(struct paging_desc_entry)) == 0;
}

int paging_map(struct paging_desc *desc, void *virt, void *phys, int flags)
{
	int res = 0;
	uintptr_t va = (uintptr_t)virt;
	size_t pml4_index = (va >> 39) & 0x1FF;
	size_t pdpt_index = (va >> 30) & 0x1FF;
	size_t pdt_index = (va >> 21) & 0x1FF;
	size_t pt_index = (va >> 12) & 0x1FF;

	struct paging_desc_entry *pml4_entry = &desc->pml->entries[pml4_index];
	if (paging_null_entry(pml4_entry))
	{
		void *new_pdpt = kzalloc(sizeof(struct paging_desc_entry) * PAGING_TOTAL_ENTRIES_PER_TABLE);
		pml4_entry->address = ((uintptr_t)new_pdpt) >> 12;
		pml4_entry->present = 1;
		pml4_entry->read_write = 1;
	}

	struct paging_desc_entry *pdpt_entries = (struct paging_desc_entry *)((uintptr_t)(pml4_entry->address << 12));

	struct paging_desc_entry *pdpt_entry = &pdpt_entries[pdpt_index];
	if (paging_null_entry(pdpt_entry))
	{
		void *new_pdt = kzalloc(sizeof(struct paging_desc_entry) * PAGING_TOTAL_ENTRIES_PER_TABLE);
		pdpt_entry->address = ((uintptr_t)new_pdt) >> 12;
		pdpt_entry->present = 1;
		pdpt_entry->read_write = 1;
	}

	struct paging_desc_entry *pdt_entries = (struct paging_desc_entry *)((uintptr_t)(pdpt_entry->address << 12));

	// pd entry
	struct paging_desc_entry *pdt_entry = &pdt_entries[pdt_index];
	if (paging_null_entry(pdt_entry))
	{
		void *new_pt = kzalloc(sizeof(struct paging_desc_entry) * PAGING_TOTAL_ENTRIES_PER_TABLE);
		pdt_entry->address = ((uintptr_t)new_pt) >> 12;
		pdt_entry->present = 1;
		pdt_entry->read_write = 1;
	}

	struct paging_desc_entry *pt_entries = (struct paging_desc_entry *)((uintptr_t)(pdt_entry->address << 12));

	// final page table entry
	struct paging_desc_entry *pt_entry = &pt_entries[pt_index];
	if (!paging_null_entry(pt_entry))
	{
		paging_invalidate_tlb_entry((uintptr_t)virt); // invalidate cache
	}

	pt_entry->address = ((uintptr_t)phys) >> 12;
	pt_entry->present = (flags & PAGING_IS_PRESENT) ? 1 : 0;
	pt_entry->read_write = (flags & PAGING_IS_WRITEABLE) ? 1 : 0;

	return res;
}

int paging_map_e820_memory_regions(struct paging_desc *desc)
{
	paging_map_to(desc, (void *)0x00, (void *)0x00, (void *)0x100000, PAGING_IS_PRESENT | PAGING_IS_WRITEABLE);
	size_t total_entries = e820_total_entries();
	for (size_t i = 0; i < total_entries; i++)
	{
		struct e820_entry *entry = e820_entry(i);
		if (entry->type == 1) // only map usable RAM
		{
			void *base_addr = (void *)(void *)entry->base_addr;
			void *end_addr = (void *)(void *)(entry->base_addr + entry->length);
			if (!paging_is_aligned(base_addr))
			{
				base_addr = paging_align_address(base_addr);
			}

			if (!paging_is_aligned(end_addr))
			{
				end_addr = paging_align_to_lower_page(end_addr);
			}

			paging_map_to(desc, base_addr, base_addr, end_addr, PAGING_IS_PRESENT | PAGING_IS_WRITEABLE);
		}
	}

	return 0;
}

int paging_map_range(struct paging_desc *desc, void *virt, void *phys, size_t count, int flags)
{
	int res = 0;
	for (size_t i = 0; i < count; i++)
	{
		res = paging_map(desc, virt, phys, flags);
		if (res < 0)
			break;
		virt += PAGING_PAGE_SIZE;
		phys += PAGING_PAGE_SIZE;
	}

	return res;
}

int paging_map_to(struct paging_desc *desc, void *virt, void *phys, void *phys_end, int flags)
{
	int res = 0;
	if ((uintptr_t)virt % PAGING_PAGE_SIZE)
	{
		res = -EINVARG;
		goto out;
	}

	if ((uintptr_t)phys % PAGING_PAGE_SIZE)
	{
		res = -EINVARG;
		goto out;
	}

	if ((uintptr_t)phys_end % PAGING_PAGE_SIZE)
	{
		res = -EINVARG;
		goto out;
	}

	if ((uintptr_t)phys_end < (uintptr_t)phys)
	{
		res = -EINVARG;
		goto out;
	}

	uint64_t total_bytes = (uintptr_t)phys_end - (uintptr_t)phys;
	size_t total_pages = total_bytes / PAGING_PAGE_SIZE;
	res = paging_map_range(desc, virt, phys, total_pages, flags);

out:
	return res;
}

// OLD --- IGNORE ---
// void paging_load_directory(uint32_t *directory);
// static uint32_t *current_directory = 0;

// struct paging_4gb_chunk *paging_new_4gb(uint8_t flags)
// {
// 	uint32_t *directory = kzalloc(sizeof(uint32_t) * PAGING_TOTAL_ENTRIES_PER_TABLE);
// 	int offset = 0;
// 	for (int i = 0; i < PAGING_TOTAL_ENTRIES_PER_TABLE; i++)
// 	{
// 		uint32_t *entry = kzalloc(sizeof(uint32_t) * PAGING_TOTAL_ENTRIES_PER_TABLE);
// 		for (int j = 0; j < PAGING_TOTAL_ENTRIES_PER_TABLE; j++)
// 		{
// 			entry[j] = (offset + (j * PAGING_PAGE_SIZE)) | flags;
// 		}
// 		offset += (PAGING_TOTAL_ENTRIES_PER_TABLE * PAGING_PAGE_SIZE);
// 		directory[i] = (uint32_t)entry | flags | PAGING_IS_WRITEABLE;
// 	}

// 	struct paging_4gb_chunk *chunk_4gb = kzalloc(sizeof(struct paging_4gb_chunk));
// 	chunk_4gb->directory_entry = directory;
// 	return chunk_4gb;
// }

// void paging_switch(struct paging_4gb_chunk *directory)
// {
// 	paging_load_directory(directory->directory_entry);
// 	current_directory = directory->directory_entry;
// }

// uint32_t *paging_4gb_chunk_get_directory(struct paging_4gb_chunk *chunk)
// {
// 	return chunk->directory_entry;
// }

// bool paging_is_aligned(void *addr)
// {
// 	return ((uint32_t)addr % PAGING_PAGE_SIZE) == 0;
// }

// int paging_get_indexes(void *virtual_address, uint32_t *directory_index_out, uint32_t *table_index_out)
// {
// 	int res = 0;
// 	if (!paging_is_aligned(virtual_address))
// 	{
// 		res = -EINVARG;
// 		goto out;
// 	}

// 	*directory_index_out = ((uint32_t)virtual_address / (PAGING_TOTAL_ENTRIES_PER_TABLE * PAGING_PAGE_SIZE));
// 	*table_index_out = ((uint32_t)virtual_address % (PAGING_TOTAL_ENTRIES_PER_TABLE * PAGING_PAGE_SIZE) / PAGING_PAGE_SIZE);
// out:
// 	return res;
// }

// int paging_set(uint32_t *directory, void *virt, uint32_t val)
// {
// 	if (!paging_is_aligned(virt))
// 	{
// 		return -EINVARG;
// 	}

// 	uint32_t directory_index = 0;
// 	uint32_t table_index = 0;
// 	int res = paging_get_indexes(virt, &directory_index, &table_index);
// 	if (res < 0)
// 	{
// 		return res;
// 	}

// 	uint32_t entry = directory[directory_index];
// 	uint32_t *table = (uint32_t *)(entry & 0xfffff000);
// 	table[table_index] = val;
// 	return res;
// }

// void paging_free_4gb(struct paging_4gb_chunk *chunk)
// {
// 	for (int i = 0; i < 1024; i++)
// 	{
// 		uint32_t entry = chunk->directory_entry[i];
// 		uint32_t *table = (uint32_t *)(entry & 0xfffff000);
// 		kfree(table);
// 	}

// 	kfree(chunk->directory_entry);
// 	kfree(chunk);
// }

// void *paging_align_address(void *ptr)
// {
// 	if ((uint32_t)ptr % PAGING_PAGE_SIZE)
// 	{
// 		return (void *)((uint32_t)ptr + PAGING_PAGE_SIZE - ((uint32_t)ptr % PAGING_PAGE_SIZE));
// 	}

// 	return ptr;
// }

// void *paging_align_to_lower_page(void *addr)
// {
// 	uint32_t _addr = (uint32_t)addr;
// 	_addr -= _addr % PAGING_PAGE_SIZE;
// 	return (void *)_addr;
// }

// int paging_map(struct paging_4gb_chunk *directory, void *virt, void *phys, int flags)
// {
// 	if (((unsigned int)virt % PAGING_PAGE_SIZE) || ((unsigned int)phys % PAGING_PAGE_SIZE))
// 	{
// 		return -EINVARG;
// 	}

// 	return paging_set(directory->directory_entry, virt, (uint32_t)phys | flags);
// }

// int paging_map_range(struct paging_4gb_chunk *directory, void *virt, void *phys, int count, int flags)
// {
// 	int res = 0;
// 	for (int i = 0; i < count; i++)
// 	{
// 		res = paging_map(directory, virt, phys, flags);
// 		if (res < 0)
// 			break;
// 		virt += PAGING_PAGE_SIZE;
// 		phys += PAGING_PAGE_SIZE;
// 	}

// 	return res;
// }

// int paging_map_to(struct paging_4gb_chunk *directory, void *virt, void *phys, void *phys_end, int flags)
// {
// 	int res = 0;
// 	if ((uint32_t)virt % PAGING_PAGE_SIZE)
// 	{
// 		res = -EINVARG;
// 		goto out;
// 	}

// 	if ((uint32_t)phys % PAGING_PAGE_SIZE)
// 	{
// 		res = -EINVARG;
// 		goto out;
// 	}

// 	if ((uint32_t)phys_end % PAGING_PAGE_SIZE)
// 	{
// 		res = -EINVARG;
// 		goto out;
// 	}

// 	if ((uint32_t)phys_end < (uint32_t)phys)
// 	{
// 		res = -EINVARG;
// 		goto out;
// 	}

// 	uint32_t total_bytes = phys_end - phys;
// 	int total_pages = total_bytes / PAGING_PAGE_SIZE;
// 	res = paging_map_range(directory, virt, phys, total_pages, flags);

// out:
// 	return res;
// }

// uint32_t paging_get(uint32_t *directory, void *virt)
// {
// 	uint32_t directory_index = 0;
// 	uint32_t table_index = 0;
// 	paging_get_indexes(virt, &directory_index, &table_index);
// 	uint32_t entry = directory[directory_index];
// 	uint32_t *table = (uint32_t *)(entry & 0xfffff000);
// 	return table[table_index];
// }

// void *paging_get_phys(uint32_t *directory, void *virt)
// {
// 	void *virt_new = (void *)paging_align_to_lower_page(virt);
// 	void *diff = (void *)((uint32_t)virt - (uint32_t)virt_new);

// 	return (void *)((paging_get(directory, virt_new) & 0xfffff000) + diff);
// }
