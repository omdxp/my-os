#include "file.h"
#include "task/task.h"
#include "task/process.h"
#include "idt/idt.h"
#include <stddef.h>

void *isr80h_command10_fopen(struct interrupt_frame *frame)
{
	int fd = 0;
	void *filename_virt_addr = NULL;
	void *mode_virt_addr = NULL;
	filename_virt_addr = task_get_stack_item(task_current(), 0);
	filename_virt_addr = task_virtual_addr_to_phys(task_current(), filename_virt_addr);
	if (!filename_virt_addr)
	{
		fd = -1;
		goto out;
	}

	mode_virt_addr = task_get_stack_item(task_current(), 1);
	mode_virt_addr = task_virtual_addr_to_phys(task_current(), mode_virt_addr);
	if (!mode_virt_addr)
	{
		fd = -1;
		goto out;
	}

	fd = process_fopen(process_current(), (const char *)filename_virt_addr, (const char *)mode_virt_addr);
	if (fd <= 0)
	{
		goto out;
	}

out:
	return (void *)(int64_t)fd;
}

void *isr80h_command11_fclose(struct interrupt_frame *frame)
{
	int64_t fd = (int64_t)task_get_stack_item(task_current(), 0);

	// close file handle
	process_fclose(process_current(), (int)fd);

	return NULL;
}

void *isr80h_command12_fread(struct interrupt_frame *frame)
{
	int res = 0;
	void *buffer_virt_addr = task_get_stack_item(task_current(), 0);
	size_t size = (size_t)(int64_t)task_get_stack_item(task_current(), 1);
	size_t count = (size_t)(int64_t)task_get_stack_item(task_current(), 2);
	long fd = (long)(int64_t)task_get_stack_item(task_current(), 3);

	res = process_fread(process_current(), buffer_virt_addr, size, count, (int)fd);
	return (void *)(int64_t)res;
}