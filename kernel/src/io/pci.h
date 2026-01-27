#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PCI_HEADER_VENDOR_OFFSET 0x00		   // Offset for Vendor ID (16 bits)
#define PCI_HEADER_DEVICE_OFFSET 0x02		   // Offset for Device ID (16 bits)
#define PCI_HEADER_COMMAND_OFFSET 0x04		   // Offset for Command Register (16 bits)
#define PCI_HEADER_STATUS_OFFSET 0x06		   // Offset for Status Register (16 bits)
#define PCI_HEADER_REVISION_ID_OFFSET 0x08	   // Offset for Revision ID (8 bits)
#define PCI_HEADER_PROG_IF_OFFSET 0x09		   // Offset for Programming Interface (8 bits)
#define PCI_HEADER_SUBCLASS_OFFSET 0x0A		   // Offset for Subclass (8 bits)
#define PCI_HEADER_CLASS_CODE_OFFSET 0x0B	   // Offset for Class Code (8 bits)
#define PCI_HEADER_CACHE_LINE_SIZE_OFFSET 0x0C // Offset for Cache Line Size (8 bits)
#define PCI_HEADER_LATENCY_TIMER_OFFSET 0x0D   // Offset for Latency Timer (8 bits)
#define PCI_HEADER_HEADER_TYPE_OFFSET 0x0E	   // Offset for Header Type (8 bits)
#define PCI_HEADER_BIST_OFFSET 0x0F			   // Offset for Built-in Self-Test (8 bits)

#define PCI_HEADER_BAR0_OFFSET 0x10 // Offset for Base Address Register 0 (32 bits)
#define PCI_HEADER_BAR1_OFFSET 0x14 // Offset for Base Address Register 1 (32 bits)
#define PCI_HEADER_BAR2_OFFSET 0x18 // Offset for Base Address Register 2 (32 bits)
#define PCI_HEADER_BAR3_OFFSET 0x1C // Offset for Base Address Register 3 (32 bits)
#define PCI_HEADER_BAR4_OFFSET 0x20 // Offset for Base Address Register 4 (32 bits)
#define PCI_HEADER_BAR5_OFFSET 0x24 // Offset for Base Address Register 5 (32 bits)

#define PCI_BRIDGE_PRIMARY_BUS_OFFSET 0x18			   // Offset for Primary Bus Number (8 bits)
#define PCI_BRIDGE_SECONDARY_BUS_OFFSET 0x19		   // Offset for Secondary Bus Number (8 bits)
#define PCI_BRIDGE_SUBORDINATE_BUS_OFFSET 0x1A		   // Offset for Subordinate Bus Number (8 bits)
#define PCI_BRIDGE_SECONDARY_LATENCY_TIMER_OFFSET 0x1B // Offset for Secondary Latency Timer (8 bits)

#define PCI_HEADER_INTERRUPT_LINE_OFFSET 0x3C // Offset for Interrupt Line (8 bits)
#define PCI_HEADER_INTERRUPT_PIN_OFFSET 0x3D  // Offset for Interrupt Pin (8 bits)

#define PCI_CFG_ADDRESS 0xCF8  // I/O port for PCI configuration address
#define PCI_DATA_ADDRESS 0xCFC // I/O port for PCI configuration data

#define PCI_BASE_CLASS(bus, slot, func) pci_cfg_read_byte(bus, slot, func, PCI_HEADER_CLASS_CODE_OFFSET)
#define PCI_BASE_SUBCLASS(bus, slot, func) pci_cfg_read_byte(bus, slot, func, PCI_HEADER_SUBCLASS_OFFSET)

enum
{
	PCI_DEVICE_IO_MEMORY = 0,
	PCI_DEVICE_IO_PORT,
};

enum
{
	PCI_DEVICE_BAR_FLAG_64BIT = 0x01,
	PCI_DEVICE_BAR_FLAG_IS_EXT = 0x02,
	PCI_DEVICE_BAR_FLAG_PREFETCHABLE = 0x04,
};

struct pci_device_bar
{
	int type;		// PCI_DEVICE_IO_MEMORY or PCI_DEVICE_IO_PORT
	uint32_t flags; // Combination of PCI_DEVICE_BAR_FLAG_*
	uint64_t addr;	// Mapped address
	uint64_t size;	// Size of the region
};

struct pci_address
{
	uint8_t bus;	  // Bus number
	uint8_t slot;	  // Slot number (dev)
	uint8_t function; // Function number
};

struct pci_class_code
{

	int base;	  // Base class code
	int subclass; // Subclass code
};

struct pci_device
{
	struct pci_address addr;	   // PCI address
	struct pci_device_bar bars[6]; // Base Address Registers

	uint16_t vendor;			 // Vendor ID
	uint16_t device_id;			 // Device ID
	struct pci_class_code class; // Class codes

	bool is_bridge;					  // Is this device a PCI bridge
	uint8_t primary_bus;			  // Primary bus number (if bridge)
	uint8_t secondary_bus;			  // Secondary bus number (if bridge)
	uint8_t subordinate_bus;		  // Subordinate bus number (if bridge)
	struct pci_device *parent_bridge; // Pointer to parent bridge device
};

struct pci_ecam_range
{
	uint16_t seg_group; // Segment group number
	uint8_t start_bus;	// Starting bus number
	uint8_t end_bus;	// Ending bus number
	uint64_t phys_base; // Physical base address of the ECAM region
	void *virt_base;	// Virtual base address of the ECAM region
};

typedef void *(*pci_ecam_map_fn_t)(uint64_t phys_addr, size_t size);
int pci_ecam_init_from_mcfg(void *mcfg_table_virt, pci_ecam_map_fn_t map_fn);
int pci_ecam_install_range(uint16_t seg_group, uint64_t phys_base, uint8_t start_bus, uint8_t end_bus, void *virt_base);
bool pci_ecam_available(void);

size_t pci_device_count(void);
uint8_t pci_cfg_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_cfg_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint32_t pci_cfg_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_cfg_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
void pci_cfg_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);

int pci_init(void);
int pci_device_get(size_t index, struct pci_device **device_out);
int pci_device_base_class(struct pci_device *device);
int pci_device_subclass(struct pci_device *device);

void bus_enable_bus_master(struct pci_device *device);