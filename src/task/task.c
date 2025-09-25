#include "task.h"
#include "memory/memory.h"
#include "memory/heap/kheap.h"
#include "memory/paging/paging.h"
#include "kernel.h"
#include "status.h"
#include "memory/paging/paging.h"
#include "process.h"
#include "string/string.h"
#include "loader/formats/elfloader.h"

// current task that is running
struct task *current_task = 0;

// task linked list
struct task *task_tail = 0;
struct task *task_head = 0;

struct task *task_current()
{
	return current_task;
}

struct task *task_get_next()
{
	if (!current_task->next)
	{
		return task_head;
	}

	return current_task->next;
}

static void task_list_remove(struct task *task)
{
	if (task->prev)
	{
		task->prev->next = task->next;
	}

	if (task == task_head)
	{
		task_head = task->next;
	}

	if (task == task_tail)
	{
		task_tail = task->prev;
	}

	if (task == current_task)
	{
		current_task = task_get_next();
	}
}

int task_free(struct task *task)
{
	paging_desc_free(task->paging_desc);
	task_list_remove(task);

	// free task data
	kfree(task);
	return 0;
}

int task_init(struct task *task, struct process *process)
{
	memset(task, 0, sizeof(struct task));

	// map entire 4gb address space to its self
	task->paging_desc = paging_desc_new(PAGING_MAP_LEVEL_4);
	if (!task->paging_desc)
	{
		return -ENOMEM;
	}

	task->registers.ip = MYOS_PROGRAM_VIRTUAL_ADDRESS;
	if (process->filetype == PROCESS_FILETYPE_ELF)
	{
		panic("ELF entry point not implemented yet\n");
		// task->registers.ip = elf_header(process->elf_file)->e_entry;
	}

	task->registers.ss = USER_DATA_SEGMENT;
	task->registers.cs = USER_CODE_SEGMENT;
	task->registers.rsp = MYOS_PROGRAM_VIRTUAL_STACK_ADDRESS_START;

	task->process = process;

	return 0;
}

struct task *task_new(struct process *process)
{
	int res = 0;
	struct task *task = kzalloc(sizeof(struct task));
	if (!task)
	{
		res = -ENOMEM;
		goto out;
	}

	res = task_init(task, process);
	if (res != MYOS_ALL_OK)
	{
		goto out;
	}

	if (task_head == 0)
	{
		task_head = task;
		task_tail = task;
		current_task = task;
		goto out;
	}

	task_tail->next = task;
	task->prev = task_tail;
	task_tail = task;

out:
	if (ISERR(res))
	{
		task_free(task);
		return ERROR(res);
	}

	return task;
}

int task_switch(struct task *task)
{
	current_task = task;
	paging_switch(task->paging_desc);
	return 0;
}

struct paging_desc *task_paging_desc(struct task *task)
{
	return task->paging_desc;
}

struct paging_desc *task_current_paging_desc()
{
	if (!current_task)
	{
		panic("task_current_paging_desc: No current task exists!\n");
	}

	return current_task->paging_desc;
}

void task_save_state(struct task *task, struct interrupt_frame *frame)
{
	task->registers.ip = frame->ip;
	task->registers.cs = frame->cs;
	task->registers.flags = frame->flags;
	task->registers.rsp = frame->rsp;
	task->registers.ss = frame->ss;
	task->registers.rax = frame->rax;
	task->registers.rbp = frame->rbp;
	task->registers.rbx = frame->rbx;
	task->registers.rcx = frame->rcx;
	task->registers.rdi = frame->rdi;
	task->registers.rdx = frame->rdx;
	task->registers.rsi = frame->rsi;
}

int copy_string_from_task(struct task *task, void *virt, void *phys, int max)
{
	if (max >= PAGING_PAGE_SIZE)
	{
		return -EINVARG;
	}

	int res = 0;
	char *tmp = kzalloc(max);
	if (!tmp)
	{
		res = -ENOMEM;
		goto out;
	}

	void *phys_tmp = paging_get_physical_address(kernel_desc(), tmp);
	struct paging_desc *task_desc = task_paging_desc(task);
	struct paging_desc_entry old_entry;
	memcpy(&old_entry, paging_get(task_desc, phys_tmp), sizeof(struct paging_desc_entry));

	int old_entry_flags = 0;
	old_entry_flags |= old_entry.read_write | old_entry.present | old_entry.user_supervisor;

	paging_map(task_desc, phys_tmp, phys_tmp, PAGING_IS_WRITEABLE | PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL);

	// switch to user page directory
	task_page_task(task);

	strncpy(tmp, virt, max);

	// switch back to kernel page directory
	kernel_page();

	strncpy(phys, tmp, max);

	// remap physical page to its old mapping
	paging_map(task_desc, phys_tmp, (void *)((uint64_t)(old_entry.address << 12)), old_entry_flags);
out:
	if (tmp)
	{
		kfree(tmp);
	}
	return res;
}

void task_current_save_state(struct interrupt_frame *frame)
{
	if (!task_current())
	{
		panic("task_current_save_state: No current task to save\n");
	}

	struct task *task = task_current();
	task_save_state(task, frame);
}

int task_page()
{
	user_registers();
	task_switch(current_task);
	return 0;
}

int task_page_task(struct task *task)
{
	user_registers();
	paging_switch(task_paging_desc(task));
	return 0;
}

void task_run_first_ever_task()
{
	if (!current_task)
	{
		panic("task_run_first_ever_task: No current task exists!\n");
	}

	task_switch(task_head);
	task_return(&task_head->registers);
}

void *task_get_stack_item(struct task *task, int index)
{
	void *res = 0;

	uint64_t *sp_ptr = (uint64_t *)task->registers.rsp;

	// switch to given task's page
	task_page_task(task);

	res = (void *)sp_ptr[index];

	// switch back to kernel's page
	kernel_page();

	return res;
}

void *task_virtual_addr_to_phys(struct task *task, void *virt)
{
	return paging_get_physical_address(task->paging_desc, virt);
}

void task_next()
{
	struct task *next_task = task_get_next();
	if (!next_task)
	{
		panic("task_next: No more tasks\n");
	}

	task_switch(next_task);
	task_return(&next_task->registers);
}
