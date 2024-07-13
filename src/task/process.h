#pragma once

#include <stdint.h>
#include "task.h"
#include "config.h"

struct process
{
	// process ID
	uint16_t id;

	char filename[MYOS_MAX_PATH];

	// main process task
	struct task *task;

	// process memory allocations (malloc)
	void *allocations[MYOS_MAX_PROGRAM_ALLOCATIONS];

	// physical pointer to process memory
	void *ptr;

	// physical pointer to stack memory
	void *stack;

	// size of data pointed by `ptr`
	uint32_t size;
};

int process_load_for_slot(const char *filename, struct process **process, int process_slot);
