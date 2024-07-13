#pragma once

#include <stdint.h>
#include "config.h"

struct registers
{
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;

	uint32_t ip;
	uint32_t cs;
	uint32_t flags;
	uint32_t esp;
	uint32_t ss;
};

struct task
{
	// page directory of task
	struct paging_4gb_chunk *page_directory;

	// registers of task when task is not running
	struct registers registers;

	// next task in linked list
	struct task *next;

	// previous task in linked list
	struct task *prev;
};

struct task *task_new();
struct task *task_current();
struct task *task_get_next();
int task_free(struct task *task);
