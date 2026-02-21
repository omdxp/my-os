#include "io/pci.h"
#include "io/io.h"
#include "memory/heap/kheap.h"
#include "lib/vector.h"
#include "status.h"
#include "kernel.h"
#include <stdbool.h>

// ECAM globals
#define PCI_ECAM_MAX_RANGES 8  // Maximum number of ECAM ranges supported
#define PCI_ECAM_BUS_SHIFT 20  // Shift for bus number in ECAM address calculation (1 MB per bus)
#define PCI_ECAM_DEV_SHIFT 15  // Shift for device number in ECAM address calculation (32 KB per device)
#define PCI_ECAM_FUNC_SHIFT 12 // Shift for function number in ECAM address calculation (4 KB per function)

static struct pci_ecam_range g_ecam[PCI_ECAM_MAX_RANGES];
static int g_ecam_count = 0;
static bool g_ecam_enabled = false;

static void pci_scan_bus(uint8_t bus, struct pci_device *parent_bridge);
static void pci_size_bar(uint8_t bus, uint8_t dev, uint8_t func, struct pci_device *device);

struct vector *pci_device_vector = NULL;

static inline uint32_t pci_cgf_addr_legacy(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
	uint16_t offset_aligned = offset & ~0x3u; // Align offset to 4 bytes
	return 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | offset_aligned;
}

static inline volatile uint8_t *pci_ecam_cfg_ptr(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset)
{
	if (!g_ecam_enabled)
	{
		return NULL; // ECAM not enabled
	}

	if (offset >= 4096)
	{
		return NULL; // Offset out of bounds for ECAM
	}

	for (int i = 0; i < g_ecam_count; i++)
	{
		struct pci_ecam_range *range = &g_ecam[i];
		if (bus >= range->start_bus && bus <= range->end_bus)
		{
			size_t bus_offset = ((size_t)(bus - range->start_bus) << PCI_ECAM_BUS_SHIFT);
			size_t dev_offset = ((size_t)slot << PCI_ECAM_DEV_SHIFT);
			size_t func_offset = ((size_t)func << PCI_ECAM_FUNC_SHIFT);
			size_t total_offset = bus_offset + dev_offset + func_offset + offset;
			return (volatile uint8_t *)(range->virt_base + total_offset);
		}
	}

	return NULL; // Bus not covered by any ECAM range
}

int pci_ecam_install_range(uint16_t seg_group, uint64_t phys_base, uint8_t start_bus, uint8_t end_bus, void *virt_base)
{
	if (g_ecam_count >= PCI_ECAM_MAX_RANGES)
	{
		return -ENOMEM; // No more space for ECAM ranges
	}

	if (!virt_base || start_bus > end_bus)
	{
		return -EINVAL; // Invalid parameters
	}

	struct pci_ecam_range *range = &g_ecam[g_ecam_count++];
	range->seg_group = seg_group;
	range->phys_base = phys_base;
	range->virt_base = virt_base;
	range->start_bus = start_bus;
	range->end_bus = end_bus;

	g_ecam_enabled = true; // Enable ECAM support

	return 0; // Success
}

#pragma pack(push, 1) // push alignment to 1 byte to match MCFG table layout
typedef struct
{
	char signature[4];				// "MCFG"
	uint32_t length;				// Length of the MCFG table
	uint8_t revision;				// Revision number
	uint8_t checksum;				// Checksum of the MCFG table
	char oem_id[6];					// OEM ID string
	char oem_table_id[8];			// OEM Table ID string
	uint32_t oem_revision;			// OEM revision number
	uint32_t asl_compiler_id;		// ASL compiler ID
	uint32_t asl_compiler_revision; // ASL compiler revision
} acpi_sdt_header_t;

typedef struct
{
	acpi_sdt_header_t header; // Standard ACPI table header
	uint64_t reserved;		  // Reserved field (must be zero)
} acpi_mcfg_t;

typedef struct
{
	uint64_t base_addr; // Base address of the ECAM region
	uint16_t seg_group; // Segment group number
	uint8_t start_bus;	// Starting bus number
	uint8_t end_bus;	// Ending bus number
	uint32_t reserved;	// Reserved field (must be zero)
} acpi_mcfg_entry_t;
#pragma pack(pop) // restore default packing

int pci_ecam_init_from_mcfg(void *mcfg_table_virt, pci_ecam_map_fn_t mapper)
{
	if (!mcfg_table_virt || !mapper)
	{
		return -EINVAL; // Invalid parameters
	}

	acpi_mcfg_t *mcfg = (acpi_mcfg_t *)mcfg_table_virt;
	if (mcfg->header.length < sizeof(acpi_mcfg_t))
	{
		return -EINVAL; // MCFG table too small
	}

	size_t entries_bytes = (size_t)mcfg->header.length - sizeof(acpi_mcfg_t);

	size_t count = entries_bytes / sizeof(acpi_mcfg_entry_t);
	acpi_mcfg_entry_t *ent = (acpi_mcfg_entry_t *)((uint8_t *)mcfg + sizeof(acpi_mcfg_t));
	int installed = 0;
	for (size_t i = 0; i < count; i++)
	{
		uint8_t start = ent[i].start_bus;
		uint8_t end = ent[i].end_bus;
		if (start > end)
		{
			continue; // Skip invalid entry
		}

		size_t buses = (size_t)(end - start + 1);
		size_t map_size = buses << PCI_ECAM_BUS_SHIFT;								// Calculate size needed for this range
		uint64_t phys = ent[i].base_addr + ((uint64_t)start << PCI_ECAM_BUS_SHIFT); // Calculate physical address for the start bus
		void *virt = mapper(phys, map_size);										// Map the ECAM region into virtual memory
		if (!virt)
		{
			continue; // Mapping failed, skip this entry
		}

		if (pci_ecam_install_range(ent[i].seg_group, phys, start, end, virt) == 0)
		{
			installed++;
		}
	}

	return (installed > 0) ? 0 : -ENOENT; // Return success if at least one range was installed, otherwise return error
}

bool pci_ecam_available(void)
{
	return g_ecam_enabled;
}

uint32_t pci_cfg_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
	uint8_t off = offset & ~0x3u; // Align offset to 4 bytes
	volatile uint8_t *ptr = pci_ecam_cfg_ptr(bus, slot, func, off);
	if (ptr)
	{
		return *(volatile uint32_t *)ptr;
	}

	// Fallback to legacy I/O port access if ECAM is not available
	uint32_t addr = pci_cgf_addr_legacy(bus, slot, func, off);
	outdw(PCI_CFG_ADDRESS, addr);
	return insdw(PCI_DATA_ADDRESS);
}

uint16_t pci_cfg_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
	uint8_t off = offset & ~0x3u; // Align offset to 4 bytes
	volatile uint8_t *ptr = pci_ecam_cfg_ptr(bus, slot, func, off);
	if (ptr)
	{
		uint32_t val = *(volatile uint32_t *)ptr;
		return (uint16_t)((val >> ((offset & 2u) * 8)) & 0xFFFFu); // Extract the correct word based on offset
	}

	uint32_t addr = pci_cgf_addr_legacy(bus, slot, func, off);
	outdw(PCI_CFG_ADDRESS, addr);
	uint32_t data = insdw(PCI_DATA_ADDRESS);
	return (uint16_t)((data >> ((offset & 2u) * 8)) & 0xFFFFu); // Extract the correct word based on offset
}

uint8_t pci_cfg_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
	uint32_t val = pci_cfg_read_dword(bus, slot, func, offset & ~0x3u); // Read the aligned dword
	return (uint8_t)(val >> ((offset & 3u) * 8));						// Extract the correct byte based on offset
}

void pci_cfg_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value)
{
	uint8_t off = offset & ~0x3u; // Align offset to 4 bytes
	volatile uint8_t *ptr = pci_ecam_cfg_ptr(bus, slot, func, off);
	if (ptr)
	{
		*(volatile uint32_t *)ptr = value;
		return;
	}

	uint32_t addr = pci_cgf_addr_legacy(bus, slot, func, off);
	outdw(PCI_CFG_ADDRESS, addr);
	outdw(PCI_DATA_ADDRESS, value);
}

void pci_cfg_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value)
{
	uint8_t off = offset & ~0x3u; // Align offset to 4 bytes
	volatile uint8_t *ptr = pci_ecam_cfg_ptr(bus, slot, func, off);
	if (ptr)
	{
		volatile uint32_t *pdw = (volatile uint32_t *)ptr;
		uint32_t current = *pdw;
		if ((offset & 2u) == 0)
		{
			current = (current & 0xFFFF0000u) | (uint32_t)value; // Update lower word
		}
		else
		{
			current = (current & 0x0000FFFFu) | ((uint32_t)value << 16); // Update upper word
		}

		*pdw = current;
		return;
	}

	uint32_t addr = pci_cgf_addr_legacy(bus, slot, func, off);
	outdw(PCI_CFG_ADDRESS, addr);
	uint32_t current = insdw(PCI_DATA_ADDRESS);
	if ((offset & 2u) == 0)
	{
		current = (current & 0xFFFF0000u) | (uint32_t)value; // Update lower word
	}
	else
	{
		current = (current & 0x0000FFFFu) | ((uint32_t)value << 16); // Update upper word
	}
	outdw(PCI_CFG_ADDRESS, addr);
	outdw(PCI_DATA_ADDRESS, current);
}

static void pci_size_bars(uint8_t bus, uint8_t dev, uint8_t func, struct pci_device *device)
{
	uint8_t hdr = pci_cfg_read_byte(bus, dev, func, PCI_HEADER_HEADER_TYPE_OFFSET) & 0x7Fu;
	int bar_count = (hdr == 0x00) ? 6 : (hdr == 0x01) ? 2
													  : 0; // Determine number of BARs based on header type
	uint16_t cmd_orig = pci_cfg_read_word(bus, dev, func, PCI_HEADER_COMMAND_OFFSET);
	pci_cfg_write_word(bus, dev, func, PCI_HEADER_COMMAND_OFFSET, (uint16_t)(cmd_orig & ~(uint16_t)0x0007u)); // Disable memory and I/O access to prevent side effects during BAR probing

	for (int i = 0; i < bar_count; i++)
	{
		struct pci_device_bar *bar = &device->bars[i];
		bar->flags = 0; // Clear flags before probing
		bar->addr = 0;	// Clear address before probing
		bar->size = 0;	// Clear size before probing

		uint8_t off = PCI_HEADER_BAR0_OFFSET + (i * 4);
		uint32_t lo_orig = pci_cfg_read_dword(bus, dev, func, off);
		bool is_io = (lo_orig & 0x1u) != 0; // Check if BAR is I/O or memory
		bar->type = is_io ? PCI_DEVICE_IO_PORT : PCI_DEVICE_IO_MEMORY;
		if (!is_io && (lo_orig & 0x8u))
		{
			bar->flags |= PCI_DEVICE_BAR_FLAG_PREFETCHABLE; // Mark as prefetchable if applicable
		}

		if (is_io)
		{
			uint32_t base = lo_orig & ~0x3u;					   // Mask out the I/O flag and alignment bits
			pci_cfg_write_dword(bus, dev, func, off, 0xFFFFFFFFu); // Write all 1s to probe size
			uint32_t szv = pci_cfg_read_dword(bus, dev, func, off);
			pci_cfg_write_dword(bus, dev, func, off, lo_orig); // Restore original value

			uint32_t masked = szv & ~0x3u; // Mask out the I/O flag and alignment bits
			if (!masked)
			{
				bar->size = 0; // No size if masked value is zero
				continue;
			}

			uint64_t mask = (uint64_t)masked;
			bar->addr = (uint64_t)base;
			bar->size = ~mask + 1u;
		}
		else
		{
			uint32_t type_bits = (lo_orig >> 1) & 0x3u; // Extract type bits to determine if it's a 64-bit BAR
			bool is_64bit = (type_bits == 0x2u);
			if (is_64bit && (i + 1) < bar_count)
			{
				uint32_t hi_orig = pci_cfg_read_dword(bus, dev, func, off + 4);

				pci_cfg_write_dword(bus, dev, func, off, 0xFFFFFFFFu); // Write all 1s to probe size
				pci_cfg_write_dword(bus, dev, func, off + 4, 0xFFFFFFFFu);
				uint32_t lo_sz = pci_cfg_read_dword(bus, dev, func, off);
				uint32_t hi_sz = pci_cfg_read_dword(bus, dev, func, off + 4);
				pci_cfg_write_dword(bus, dev, func, off, lo_orig); // Restore original values
				pci_cfg_write_dword(bus, dev, func, off + 4, hi_orig);

				uint64_t base64 = ((uint64_t)hi_orig << 32) | ((uint64_t)(lo_orig & ~0xFu)); // Combine high and low parts, masking out type bits
				uint64_t mask64 = ((uint64_t)hi_sz << 32) | ((uint64_t)(lo_sz & ~0xFu));	 // Combine high and low size values, masking out type bits
				if (!mask64)
				{
					bar->size = 0; // No size if masked value is zero
					goto restore64;
				}

				bar->addr = base64;
				bar->size = ~mask64 + 1u;
				bar->flags |= PCI_DEVICE_BAR_FLAG_64BIT; // Mark as 64-bit BAR

			restore64:
				i++; // Skip the next BAR since it's part of this 64-bit BAR
				if (i < bar_count)
				{
					struct pci_device_bar *ext = &device->bars[i];
					ext->type = bar->type;					 // Set the same type for the next BAR
					ext->flags = PCI_DEVICE_BAR_FLAG_IS_EXT; // Mark as an extension of the previous BAR
					ext->addr = base64 >> 32;				 // Store the upper 32 bits in the next BAR's address field
					ext->size = 0;							 // Size is only stored in the first BAR for 64-bit BARs
				}
			}
			else
			{
				uint32_t base = lo_orig & ~0xFu;					   // Mask out type bits and alignment bits
				pci_cfg_write_dword(bus, dev, func, off, 0xFFFFFFFFu); // Write all 1s to probe size
				uint32_t szv = pci_cfg_read_dword(bus, dev, func, off);
				pci_cfg_write_dword(bus, dev, func, off, lo_orig); // Restore original value

				uint32_t masked = szv & ~0xFu; // Mask out type bits and alignment bits
				if (!masked)
				{
					bar->size = 0; // No size if masked value is zero
					continue;
				}

				uint64_t mask = (uint64_t)masked;
				bar->addr = (uint64_t)base;
				bar->size = ~mask + 1u;
			}
		}
	}

	pci_cfg_write_word(bus, dev, func, PCI_HEADER_COMMAND_OFFSET, cmd_orig); // Restore original command register value to re-enable access
}

static void pci_scan_bus(uint8_t bus, struct pci_device *parent_bridge)
{
	for (int dev = 0; dev < 32; dev++)
	{
		uint16_t vendor0 = pci_cfg_read_word(bus, dev, 0, PCI_HEADER_VENDOR_OFFSET);
		if (vendor0 == 0xFFFFu)
		{
			continue; // No device present in this slot
		}

		uint8_t hdr0 = pci_cfg_read_byte(bus, dev, 0, PCI_HEADER_HEADER_TYPE_OFFSET);
		int func_limit = (hdr0 & 0x80u) ? 8 : 1; // Check if it's a multi-function device
		for (int func = 0; func < func_limit; func++)
		{
			uint16_t vendor = pci_cfg_read_word(bus, dev, func, PCI_HEADER_VENDOR_OFFSET);
			if (vendor == 0xFFFFu)
			{
				continue; // No function present at this slot/function
			}

			struct pci_device *device = kzalloc(sizeof(struct pci_device));
			if (!device)
			{
				panic("pci_scan_bus: Failed to allocate memory for pci_device\n");
			}

			device->addr.bus = (uint8_t)bus;
			device->addr.slot = (uint8_t)dev;
			device->addr.function = (uint8_t)func;
			device->vendor = vendor;
			device->device_id = pci_cfg_read_word(bus, dev, func, PCI_HEADER_DEVICE_OFFSET);

			uint32_t classreg = pci_cfg_read_dword(bus, dev, func, PCI_HEADER_REVISION_ID_OFFSET);
			device->class.base = (classreg >> 24) & 0xFFu;
			device->class.subclass = (classreg >> 16) & 0xFFu;

			device->parent_bridge = parent_bridge;
			device->is_bridge = bus;
			device->secondary_bus = 0;
			device->subordinate_bus = 0;
			pci_size_bars(bus, dev, func, device);

			vector_push(pci_device_vector, &device); // Add the device to the global vector of PCI devices
			uint8_t hdr = pci_cfg_read_byte(bus, dev, func, PCI_HEADER_HEADER_TYPE_OFFSET) & 0x7Fu;
			if (hdr == 0x01u) // If this device is a PCI-to-PCI bridge, we need to scan the secondary bus
			{
				device->is_bridge = true;
				device->primary_bus = pci_cfg_read_byte(bus, dev, func, PCI_BRIDGE_PRIMARY_BUS_OFFSET);
				device->secondary_bus = pci_cfg_read_byte(bus, dev, func, PCI_BRIDGE_SECONDARY_BUS_OFFSET);
				device->subordinate_bus = pci_cfg_read_byte(bus, dev, func, PCI_BRIDGE_SUBORDINATE_BUS_OFFSET);
				if (device->secondary_bus != 0 && device->secondary_bus <= device->subordinate_bus)
				{
					pci_scan_bus(device->secondary_bus, device); // Recursively scan the secondary bus of the bridge
				}
			}
		}
	}
}

int pci_init(void)
{
	pci_device_vector = vector_new(sizeof(struct pci_device *), 16, 0);
	if (!pci_device_vector)
	{
		return -ENOMEM; // Failed to create vector for PCI devices
	}

	pci_scan_bus(0, NULL); // Start scanning from bus 0 with no parent bridge
	return 0;
}

size_t pci_device_count(void)
{
	return vector_count(pci_device_vector);
}

int pci_device_base_class(struct pci_device *device)
{
	return device->class.base;
}

int pci_device_subclass(struct pci_device *device)
{
	return device->class.subclass;
}

int pci_device_get(size_t index, struct pci_device **out)
{
	if (index >= pci_device_count())
	{
		return -EOUTOFRANGE; // Index out of range
	}

	return vector_at(pci_device_vector, index, out, sizeof(*out));
}

static void pci_enable_upstream_path(struct pci_device *bridge, bool need_io, bool need_mem)
{
	for (struct pci_device *cur = bridge; cur; cur = cur->parent_bridge)
	{
		uint16_t cmd = pci_cfg_read_word(cur->addr.bus, cur->addr.slot, cur->addr.function, PCI_HEADER_COMMAND_OFFSET);
		cmd |= 0x0004u; // BME
		if (need_mem)
		{
			cmd |= 0x0002u; // MSE
		}

		if (need_io)
		{
			cmd |= 0x0001u; // IOSE
		}

		pci_cfg_write_word(cur->addr.bus, cur->addr.slot, cur->addr.function, PCI_HEADER_COMMAND_OFFSET, cmd);
	}
}

void pci_enable_bus_master(struct pci_device *device)
{
	bool need_mem = false;
	bool need_io = false;
	for (int i = 0; i < 6; i++)
	{
		if (device->bars[i].size == 0)
		{
			continue; // Skip BARs with no size
		}

		if (device->bars[i].flags & PCI_DEVICE_BAR_FLAG_IS_EXT)
		{
			continue; // Skip extended BARs since they don't have their own flags
		}

		if (device->bars[i].type == PCI_DEVICE_IO_MEMORY)
		{
			need_mem = true;
		}
		else
		{
			need_io = true;
		}
	}

	uint16_t cmd = pci_cfg_read_word(device->addr.bus, device->addr.slot, device->addr.function, PCI_HEADER_COMMAND_OFFSET);
	cmd |= 0x0004u; // BME
	if (need_mem)
	{
		cmd |= 0x0002u; // MSE
	}

	if (need_io)
	{
		cmd |= 0x0001u; // IOSE
	}

	pci_cfg_write_word(device->addr.bus, device->addr.slot, device->addr.function, PCI_HEADER_COMMAND_OFFSET, cmd);

	if (device->parent_bridge)
	{
		pci_enable_upstream_path(device->parent_bridge, need_io, need_mem); // Ensure all upstream bridges have the necessary access enabled
	}
}