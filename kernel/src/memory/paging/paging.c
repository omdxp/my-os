#include "paging.h"
#include "memory/heap/kheap.h"
#include "memory/memory.h"
#include "memory/heap/heap.h"
#include "status.h"
#include "kernel.h"

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

static bool paging_null_entry(struct paging_desc_entry *entry)
{
	struct paging_desc_entry null_entry = {0};
	return memcmp(entry, &null_entry, sizeof(struct paging_desc_entry)) == 0;
}

void paging_desc_entry_free(struct paging_desc_entry *table_entry, paging_map_level_t level)
{
	if (paging_null_entry(table_entry))
	{
		return;
	}

	if (level == 0)
	{
		panic("paging_desc_entry_free: Cannot free level 0 entry directly\n");
	}

	if (level > PAGING_MAP_LEVEL_4)
	{
		panic("paging_desc_entry_free: Invalid paging level\n");
	}

	if (level > 1)
	{
		for (size_t i = 0; i < PAGING_TOTAL_ENTRIES_PER_TABLE; i++)
		{
			struct paging_desc_entry *entry = &table_entry[i];
			if (!paging_null_entry(entry))
			{
				struct paging_desc_entry *child_entry = (struct paging_desc_entry *)((uintptr_t)(entry->address) << 12);
				if (child_entry)
				{
					paging_desc_entry_free(child_entry, level - 1);
				}
			}
		}
	}

	kfree(table_entry);
}

void paging_desc_free(struct paging_desc *desc)
{
	paging_map_level_t level = desc->level;
	for (size_t i = 0; i < PAGING_TOTAL_ENTRIES_PER_TABLE; i++)
	{
		struct paging_desc_entry *entry = &desc->pml->entries[i];
		if (!paging_null_entry(entry))
		{
			struct paging_desc_entry *child_entry = (struct paging_desc_entry *)(((uintptr_t)(entry->address) << 12));
			if (child_entry)
			{
				paging_desc_entry_free(child_entry, level - 1);
			}
		}
	}

	kfree(desc->pml);
	kfree(desc);
}

static bool paging_map_level_is_valid(paging_map_level_t level)
{
	return level == PAGING_MAP_LEVEL_4;
}

struct paging_desc *paging_current_descriptor()
{
	return current_paging_desc;
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
		pml4_entry->user_supervisor = 1;
	}

	struct paging_desc_entry *pdpt_entries = (struct paging_desc_entry *)((uintptr_t)(pml4_entry->address) << 12);

	struct paging_desc_entry *pdpt_entry = &pdpt_entries[pdpt_index];
	if (paging_null_entry(pdpt_entry))
	{
		void *new_pdt = kzalloc(sizeof(struct paging_desc_entry) * PAGING_TOTAL_ENTRIES_PER_TABLE);
		pdpt_entry->address = ((uintptr_t)new_pdt) >> 12;
		pdpt_entry->present = 1;
		pdpt_entry->read_write = 1;
		pdpt_entry->user_supervisor = 1;
	}

	struct paging_desc_entry *pdt_entries = (struct paging_desc_entry *)((uintptr_t)(pdpt_entry->address) << 12);

	// pd entry
	struct paging_desc_entry *pdt_entry = &pdt_entries[pdt_index];
	if (paging_null_entry(pdt_entry))
	{
		void *new_pt = kzalloc(sizeof(struct paging_desc_entry) * PAGING_TOTAL_ENTRIES_PER_TABLE);
		pdt_entry->address = ((uintptr_t)new_pt) >> 12;
		pdt_entry->present = 1;
		pdt_entry->read_write = 1;
		pdt_entry->user_supervisor = 1;
	}

	struct paging_desc_entry *pt_entries = (struct paging_desc_entry *)((uintptr_t)(pdt_entry->address) << 12);

	// final page table entry
	struct paging_desc_entry *pt_entry = &pt_entries[pt_index];
	if (!paging_null_entry(pt_entry))
	{
		paging_invalidate_tlb_entry((uintptr_t)virt); // invalidate cache
	}

	pt_entry->address = ((uintptr_t)phys) >> 12;
	pt_entry->present = (flags & PAGING_IS_PRESENT) ? 1 : 0;
	pt_entry->read_write = (flags & PAGING_IS_WRITEABLE) ? 1 : 0;
	pt_entry->user_supervisor = (flags & PAGING_ACCESS_FROM_ALL) ? 1 : 0;

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
			void *base_addr = (void *)entry->base_addr;
			void *end_addr = (void *)(entry->base_addr + entry->length);
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

	uint64_t total_bytes = phys_end - phys;
	size_t total_pages = total_bytes / PAGING_PAGE_SIZE;
	res = paging_map_range(desc, virt, phys, total_pages, flags);

out:
	return res;
}

struct paging_desc_entry *paging_get(struct paging_desc *desc, void *virt)
{
	uintptr_t va = (uintptr_t)virt;
	size_t pml4_index = (va >> 39) & 0x1FF;
	size_t pdpt_index = (va >> 30) & 0x1FF;
	size_t pdt_index = (va >> 21) & 0x1FF;
	size_t pt_index = (va >> 12) & 0x1FF;

	struct paging_desc_entry *pml4_entry = &desc->pml->entries[pml4_index];
	if (paging_null_entry(pml4_entry))
	{
		return NULL;
	}

	struct paging_desc_entry *pdpt_entries = (struct paging_desc_entry *)(((uintptr_t)(pml4_entry->address)) << 12);
	if (paging_null_entry(pdpt_entries))
	{
		return NULL;
	}

	struct paging_desc_entry *pdpt_entry = &pdpt_entries[pdpt_index];
	if (paging_null_entry(pdpt_entry))
	{
		return NULL;
	}

	struct paging_desc_entry *pdt_entries = (struct paging_desc_entry *)(((uintptr_t)(pdpt_entry->address)) << 12);
	if (paging_null_entry(pdt_entries))
	{
		return NULL;
	}

	struct paging_desc_entry *pdt_entry = &pdt_entries[pdt_index];
	if (paging_null_entry(pdt_entry))
	{
		return NULL;
	}

	struct paging_desc_entry *pt_entries = (struct paging_desc_entry *)(((uintptr_t)(pdt_entry->address)) << 12);
	if (paging_null_entry(pt_entries))
	{
		return NULL;
	}

	struct paging_desc_entry *pt_entry = &pt_entries[pt_index];
	return pt_entry;
}

void *paging_get_physical_address(struct paging_desc *desc, void *virt)
{
	struct paging_desc_entry *desc_entry = paging_get(desc, virt);
	if (!desc_entry)
	{
		return NULL;
	}

	uint64_t physical_base = ((uint64_t)desc_entry->address) << 12;
	uint64_t offset = (uint64_t)virt & 0xFFF;

	uint64_t full_address = physical_base + offset;
	return (void *)full_address;
}
