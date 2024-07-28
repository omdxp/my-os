#pragma once

#include <stdint.h>
#include "task.h"
#include "config.h"
#include <stddef.h>
#include <stdbool.h>

#define PROCESS_FILETYPE_ELF 0
#define PROCESS_FILETYPE_BIN 1
typedef unsigned char PROCESS_FILETYPE;

struct process
{
	// process ID
	uint16_t id;

	char filename[MYOS_MAX_PATH];

	// main process task
	struct task *task;

	// process memory allocations (malloc)
	void *allocations[MYOS_MAX_PROGRAM_ALLOCATIONS];

	PROCESS_FILETYPE filetype;

	union
	{
		// physical pointer to process memory
		void *ptr;
		struct elf_file *elf_file;
	};

	// physical pointer to stack memory
	void *stack;

	// size of data pointed by `ptr`
	uint32_t size;

	// keyboard buffer
	struct keyboard_buffer
	{
		char buffer[MYOS_KEYBOARD_BUFFER_SIZE];
		int tail;
		int head;
	} keyboard;
};

int process_switch(struct process *process);
int process_load_switch(const char *filename, struct process **process);
int process_load_for_slot(const char *filename, struct process **process, int process_slot);
int process_load(const char *filename, struct process **process);
struct process *process_current();
struct process *process_get(int process_id);
void *process_malloc(struct process *process, size_t size);
void process_free(struct process *process, void *ptr);
