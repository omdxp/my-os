#pragma once

#include <stdint.h>
#include "config.h"
#include "idt/idt.h"

struct registers
{
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rbp;
	uint64_t rbx;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rax;

	uint64_t ip;
	uint64_t cs;
	uint64_t flags;
	uint64_t rsp;
	uint64_t ss;
};

struct process;

struct task
{
	// page directory of task
	struct paging_desc *paging_desc;

	// registers of task when task is not running
	struct registers registers;

	// process of task
	struct process *process;

	// next task in linked list
	struct task *next;

	// previous task in linked list
	struct task *prev;
};

struct task *task_new(struct process *process);
struct task *task_current();
struct task *task_get_next();
int task_free(struct task *task);

int task_switch(struct task *task);
int task_page();
int task_page_task(struct task *task);

void task_run_first_ever_task();
void task_return(struct registers *regs);
void restore_general_purpose_registers(struct registers *regs);
void user_registers();

void task_current_save_state(struct interrupt_frame *frame);
int copy_string_from_task(struct task *task, void *virt, void *phys, int max);
void *task_get_stack_item(struct task *task, int index);
void *task_virtual_addr_to_phys(struct task *task, void *virt);
void task_next();
struct paging_desc *task_paging_desc(struct task *task);
struct paging_desc *task_current_paging_desc();
