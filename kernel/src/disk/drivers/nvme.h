#pragma once

#include <stdint.h>
#include <stddef.h>
#include "disk.h"
#include "driver.h"

#define NVME_SECTOR_SIZE 512

#define NVME_PCI_BASE_CLASS 0x01
#define NVME_PCI_SUBCLASS 0x08

#define NVME_BASE_REGISTER_CAP 0x00
#define NVME_BASE_REGISTER_VS 0x08
#define NVME_BASE_REGISTER_INTMS 0x0C
#define NVME_BASE_REGISTER_INTMC 0x10
#define NVME_BASE_REGISTER_CC 0x14
#define NVME_BASE_REGISTER_CSTS 0x1C
#define NVME_BASE_REGISTER_NSSR 0x20
#define NVME_BASE_REGISTER_AQA 0x24
#define NVME_BASE_REGISTER_ASQ 0x28
#define NVME_BASE_REGISTER_ACQ 0x30

#define NVME_SQTDBL_OFFSET(qid, dstrd) (0x1000u + ((2u * (qid)) * (4u << (dstrd))))
#define NVME_CQTDBL_OFFSET(qid, dstrd) (0x1000u + ((2u * (qid) + 1u) * (4u << (dstrd))))

#define NVME_ADMIN_SUBMISSION_QUEUE_TOTAL_ENTRIES 64u
#define NVME_ADMIN_COMPLETION_QUEUE_TOTAL_ENTRIES 64u

#define NVME_OPCODE_READ 0x02
#define NVME_OPCODE_WRITE 0x01

typedef uint32_t NVME_COMMAND_BITS;
#define NVME_COMMAND_BITS_BUILD(opcode, fused, psdt, iden) \
	(((uint32_t)(opcode) & 0xFFu) |                        \
	 (((uint32_t)(fused) & 0x03u) << 8) |                  \
	 (((uint32_t)(psdt) & 0x03u) << 14) |                  \
	 (((uint32_t)(iden) & 0xFFFu) << 16))

struct nvme_submission_queue_entry
{
	NVME_COMMAND_BITS command;
	uint32_t nsid;
	uint64_t reserved;
	uint32_t metadata_ptr1;
	uint32_t metadata_ptr2;
	uint32_t data_ptr1;
	uint32_t data_ptr2;
	uint32_t data_ptr3;
	uint32_t data_ptr4;
	uint32_t command_cdw[6];
} __attribute__((packed));

#define NVME_COMPLETION_QUEUE_STATUS(e) \
	(((e)->status_phase_and_command_identifier >> 17) & 0x7FFFu)

struct nvme_completion_queue_entry
{
	uint32_t command_specific;
	uint32_t command_specific2;
	uint32_t sq_iden_and_head_ptr;
	uint32_t status_phase_and_command_identifier;
} __attribute__((packed));

struct pci_device;

struct nvme_disk_driver_private
{
	struct pci_device *device;
	void *base_address_nvme;
	uint8_t doorbell_stride;
	uint32_t nsid;
	uint8_t admin_cq_phase;
	struct
	{
		struct nvme_submission_queue_entry *ptr;
		uint16_t tail, size;
	} submission_queue;

	struct
	{
		struct nvme_completion_queue_entry *ptr;
		uint16_t head, size;
		uint8_t phase;
	} completion_queue;

	struct
	{
		struct nvme_submission_queue_entry *ptr;
		uint16_t tail, size;
	} io_submission_queue;

	struct
	{
		struct nvme_completion_queue_entry *ptr;
		uint16_t head, size;
		uint8_t phase;
	} io_completion_queue;
};

struct disk_driver *nvme_driver_init(void);