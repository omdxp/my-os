#pragma once

#include <stdint.h>
#include <stddef.h>

#define GPT_MASTER_BOOT_RECORD_LBA 0
#define GPT_PARTITION_TABLE_HEADER_LBA 1
#define GPT_SIGNATURE "EFI PART"

struct gpt_partition_table_header
{
	char signature[8];
	uint32_t revision;
	uint32_t header_size;
	uint32_t cr32_checksum;
	uint32_t reserved1;
	uint64_t lba;				   // current LBA
	uint64_t lba_alternate;		   // alternate LBA
	uint64_t data_block_lba;	   // first usable LBA for partitions
	uint64_t data_block_lba_end;   // last usable LBA for partitions
	char guid[16];				   // disk GUID
	uint64_t guid_array_lba_start; // starting LBA of partition entries array
	uint32_t total_array_entries;  // number of partition entries
	uint32_t array_entry_size;	   // size of a single partition entry
	uint32_t crc32_entry_array;	   // crc32 of partition entries array
	char reserved2[];			   // rest of sector is reserved
};

struct gpt_partition_entry
{
	char guid[16];					// partition type GUID
	char unique_partition_guid[16]; // unique partition GUID
	uint64_t starting_lba;			// starting LBA of the partition
	uint64_t ending_lba;			// ending LBA of the partition
	uint64_t attributes;			// partition attributes
	char partition_name[72];		// partition name (UTF-16LE)
};

int gpt_init();
