#include "process.h"
#include "memory/memory.h"
#include "memory/heap/kheap.h"
#include "status.h"
#include "task.h"
#include "fs/file.h"
#include "string/string.h"
#include "kernel.h"
#include "memory/paging/paging.h"
#include "loader/formats/elfloader.h"
#include "graphics/graphics.h"
#include "graphics/window.h"
#include "lib/vector.h"
#include <stdbool.h>

int process_close_file_handles(struct process *process);

// current process that is running
struct process *current_process = 0;

struct vector *process_vector = NULL;

void process_system_init()
{
	process_vector = vector_new(sizeof(struct process *), MYOS_MAX_PROCESSES, 0);
	for (int i = 0; i < MYOS_MAX_PROCESSES; i++)
	{
		struct process *null_process = NULL;
		vector_push(process_vector, &null_process);
	}
}

static void process_init(struct process *process)
{
	memset(process, 0, sizeof(struct process));
	process->allocations = vector_new(sizeof(struct process_allocation), 10, 0);
	process->file_handles = vector_new(sizeof(struct process_file_handle *), 4, 0);
	process->kernel_userland_ptrs_vector = vector_new(sizeof(struct userland_ptr *), 4, 0);
	process->windows = vector_new(sizeof(struct process_window *), 4, 0);
}

struct process *process_current()
{
	return current_process;
}

bool process_owns_kernel_window(struct process *process, struct window *kernel_window)
{
	size_t total_windows = vector_count(process->windows);
	for (size_t i = 0; i < total_windows; i++)
	{
		struct process_window *proc_win = NULL;
		vector_at(process->windows, i, &proc_win, sizeof(proc_win));
		if (proc_win && proc_win->kernel_win == kernel_window)
		{
			return true;
		}
	}

	return false;
}

struct process *process_get_from_kernel_window(struct window *kernel_window)
{
	size_t total_process_slots = vector_count(process_vector);
	for (size_t i = 0; i < total_process_slots; i++)
	{
		struct process *process = NULL;
		vector_at(process_vector, i, &process, sizeof(process));
		if (process)
		{
			if (process_owns_kernel_window(process, kernel_window))
			{
				return process;
			}
		}
	}

	return NULL;
}

struct process_window *process_window_get_from_user_window(struct process *process, struct process_userspace_window *user_win)
{
	size_t total_windows = vector_count(process->windows);
	for (size_t i = 0; i < total_windows; i++)
	{
		struct process_window *proc_win = NULL;
		vector_at(process->windows, i, &proc_win, sizeof(proc_win));
		if (proc_win && proc_win->user_win == user_win)
		{
			return proc_win;
		}
	}

	return NULL;
}

void process_close_windows(struct process *process)
{
	size_t total_windows = vector_count(process->windows);
	for (size_t i = 0; i < total_windows; i++)
	{
		struct process_window *proc_win = NULL;
		vector_at(process->windows, i, &proc_win, sizeof(proc_win));
		if (proc_win)
		{
			if (proc_win->kernel_win)
			{
				window_close(proc_win->kernel_win);
				proc_win->kernel_win = NULL;
			}

			if (proc_win->user_win)
			{
				process_free(process, proc_win->user_win);
				proc_win->user_win = NULL;
			}

			kfree(proc_win);
		}
	}

	vector_free(process->windows);
	process->windows = NULL;
}

struct process_window *process_window_create(struct process *process, char *title, int width, int height, int flags, int id)
{
	int res = 0;
	struct process_window *proc_win = kzalloc(sizeof(struct process_window));
	if (!proc_win)
	{
		res = -ENOMEM;
		goto out;
	}

	struct graphics_info *screen_graphics = graphics_screen_info();
	size_t abs_x = screen_graphics->width / 2 - width / 2;
	size_t abs_y = screen_graphics->height / 2 - height / 2;
	proc_win->kernel_win = window_create(screen_graphics, NULL, title, abs_x, abs_y, width, height, flags, id);
	if (!proc_win->kernel_win)
	{
		res = -ENOMEM;
		goto out;
	}

	proc_win->user_win = process_malloc(process, sizeof(struct process_userspace_window));
	if (!proc_win->user_win)
	{
		res = -ENOMEM;
		goto out;
	}

	proc_win->user_win->width = width;
	proc_win->user_win->height = height;
	strncpy(proc_win->user_win->title, title, sizeof(proc_win->user_win->title));

	// TODO: register window event handlers

	vector_push(process->windows, &proc_win);

out:
	if (res < 0)
	{
		if (proc_win->kernel_win)
		{
			window_close(proc_win->kernel_win);
			proc_win->kernel_win = NULL;
		}

		if (proc_win->user_win)
		{
			process_free(process, proc_win->user_win);
			proc_win->user_win = NULL;
		}

		kfree(proc_win);
		proc_win = NULL;
	}

	return proc_win;
}

struct process *process_get(int process_id)
{
	int res = 0;
	struct process *process_out = NULL;
	res = vector_at(process_vector, process_id, &process_out, sizeof(process_out));
	if (res < 0)
	{
		return ERROR(EINVARG);
	}

	return process_out;
}

int process_switch(struct process *process)
{
	current_process = process;
	return 0;
}

int process_find_free_allocation_index(struct process *process)
{
	int res = 0;
	bool found = false;
	size_t allocation_size = vector_count(process->allocations);
	for (size_t i = 0; i < allocation_size; i++)
	{
		struct process_allocation allocation;
		res = vector_at(process->allocations, i, &allocation, sizeof(allocation));
		if (res < 0)
		{
			break;
		}

		if (allocation.ptr == NULL)
		{
			found = true;
			res = i;
			break;
		}
	}

	if (!found)
	{
		struct process_allocation allocation = {0};
		res = vector_push(process->allocations, &allocation);
	}

	return res;
}

int process_allocation_set_map(struct process *process, int allocation_entry_index, void *ptr, size_t size)
{
	int res = paging_map_to(process->paging_desc, ptr, ptr, paging_align_address(ptr + size), PAGING_IS_PRESENT | PAGING_IS_WRITEABLE | PAGING_ACCESS_FROM_ALL);
	if (res < 0)
	{
		goto out;
	}

	struct process_allocation allocation;
	res = vector_at(process->allocations, allocation_entry_index, &allocation, sizeof(allocation));
	if (res < 0)
	{
		goto out;
	}

	allocation.ptr = ptr;
	allocation.end = ptr + size;
	allocation.size = size;
	res = vector_push(process->allocations, &allocation);

	vector_overwrite(process->allocations, allocation_entry_index, &allocation, sizeof(allocation));

out:
	return res;
}

int process_allocation_exists(struct process *process, void *ptr, size_t *index_out)
{
	int res = -ENOTFOUND;
	size_t total_allocations = vector_count(process->allocations);
	for (size_t i = 0; i < total_allocations; i++)
	{
		struct process_allocation allocation;
		res = vector_at(process->allocations, i, &allocation, sizeof(allocation));
		if (res < 0)
		{
			break;
		}

		if (allocation.ptr == ptr)
		{
			if (index_out)
			{
				*index_out = i;
			}

			res = 0;
			break;
		}
	}

	return res;
}

void *process_realloc(struct process *process, void *old_virt_ptr, size_t new_size)
{
	int res = 0;
	void *new_ptr = NULL;
	void *old_phys_ptr = NULL;
	size_t old_allocation_index = 0;
	res = process_allocation_exists(process, old_virt_ptr, &old_allocation_index);
	if (res < 0)
	{
		goto out;
	}

	old_phys_ptr = old_virt_ptr;
	if (old_phys_ptr)
	{
		old_phys_ptr = task_virtual_addr_to_phys(process->task, old_virt_ptr);
		if (!old_phys_ptr)
		{
			res = -ENOMEM;
			goto out;
		}
	}

	new_ptr = krealloc(old_phys_ptr, new_size);
	if (!new_ptr)
	{
		res = -ENOMEM;
		goto out;
	}

	res = process_allocation_set_map(process, old_allocation_index, new_ptr, new_size);
	if (res < 0)
	{
		goto out;
	}

out:
	return new_ptr;
}

void *process_malloc(struct process *process, size_t size)
{
	int res = 0;
	void *ptr = kzalloc(size);
	if (!ptr)
	{
		res = -ENOMEM;
		goto out_error;
	}

	int index = process_find_free_allocation_index(process);
	if (index < 0)
	{
		res = -ENOMEM;
		goto out_error;
	}

	res = process_allocation_set_map(process, index, ptr, size);
	if (res < 0)
	{
		res = -ENOMEM;
		goto out_error;
	}

	return ptr;

out_error:
	if (ptr)
	{
		kfree(ptr);
	}

	return 0;
}

static bool process_is_process_pointer(struct process *process, void *ptr)
{
	size_t total_allocations = vector_count(process->allocations);
	for (size_t i = 0; i < total_allocations; i++)
	{
		struct process_allocation allocation;
		int res = vector_at(process->allocations, i, &allocation, sizeof(allocation));
		if (res < 0)
		{
			break;
		}

		if (allocation.ptr == ptr)
		{
			return true;
		}
	}

	return false;
}

static void process_allocation_unjoin(struct process *process, void *ptr)
{
	size_t total_allocations = vector_count(process->allocations);
	for (size_t i = 0; i < total_allocations; i++)
	{
		struct process_allocation allocation;
		int res = vector_at(process->allocations, i, &allocation, sizeof(allocation));
		if (res < 0)
		{
			break;
		}

		if (allocation.ptr == ptr)
		{
			allocation.ptr = NULL;
			allocation.size = 0;
			allocation.end = NULL;
			vector_overwrite(process->allocations, i, &allocation, sizeof(allocation));
			return;
		}
	}
}

int process_get_allocation_by_start_addr(struct process *process, void *addr, struct process_allocation *allocation_out)
{
	size_t total_allocations = vector_count(process->allocations);
	for (size_t i = 0; i < total_allocations; i++)
	{
		struct process_allocation allocation;
		int res = vector_at(process->allocations, i, &allocation, sizeof(allocation));
		if (res < 0)
		{
			break;
		}

		if (allocation.ptr == addr)
		{
			*allocation_out = allocation;
			return 0;
		}
	}

	return -EIO;
}

int process_terminate_allocations(struct process *process)
{
	size_t total_allocations = vector_count(process->allocations);
	for (size_t i = 0; i < total_allocations; i++)
	{
		struct process_allocation allocation;
		int res = vector_at(process->allocations, i, &allocation, sizeof(allocation));
		if (res < 0)
		{
			break;
		}

		if (allocation.ptr)
		{
			process_free(process, allocation.ptr);
		}
	}

	vector_free(process->allocations);
	process->allocations = NULL;
	return 0;
}

int process_free_bin_data(struct process *process)
{
	kfree(process->ptr);
	return 0;
}

int process_free_elf_data(struct process *process)
{
	if (process->elf_file)
	{
		elf_close(process->elf_file);
	}

	return 0;
}

int process_free_program_data(struct process *process)
{
	int res = 0;
	switch (process->filetype)
	{
	case PROCESS_FILETYPE_BIN:
		res = process_free_bin_data(process);
		break;

	case PROCESS_FILETYPE_ELF:
		res = process_free_elf_data(process);
		break;

	default:
		res = -EINVARG;
	}

	return res;
}

void process_windows_closed(struct process *process, struct process_window *proc_win)
{
	// TODO: clean up and close the window
}

int process_free_process(struct process *process)
{
	int res = 0;
	process_close_windows(process);
	process_terminate_allocations(process);
	process_free_program_data(process);
	process_close_file_handles(process);

	// free allocations vector
	vector_free(process->allocations);
	process->allocations = NULL;

	// free userland pointers vector
	vector_free(process->kernel_userland_ptrs_vector);
	process->kernel_userland_ptrs_vector = NULL;

	if (process->stack)
	{
		kfree(process->stack);
		process->stack = NULL;
	}

	if (process->task)
	{
		task_free(process->task);
		process->task = NULL;
	}

	kfree(process);

out:
	return res;
}

void process_switch_to_any()
{
	size_t total_process_slots = vector_count(process_vector);
	for (size_t i = 0; i < total_process_slots; i++)
	{
		struct process *process;
		int res = vector_at(process_vector, i, &process, sizeof(&process));
		if (res < 0)
		{
			break;
		}

		if (process)
		{
			process_switch(process);
			return;
		}
	}

	panic("process_switch_to_any: No processes to switch to\n");
}

static void process_unlink(struct process *process)
{
	struct process *null_process = NULL;
	vector_overwrite(process_vector, process->id, &null_process, sizeof(&null_process));
	if (current_process == process)
	{
		process_switch_to_any();
	}
}

int process_terminate(struct process *process)
{
	process_unlink(process);
	int res = process_free_process(process);
	if (res < 0)
	{
		goto out;
	}

out:
	return res;
}

void process_get_arguments(struct process *process, int *argc, char ***argv)
{
	*argc = process->arguments.argc;
	*argv = process->arguments.argv;
}

int process_count_command_arguments(struct command_argument *root_argument)
{
	struct command_argument *current = root_argument;
	int i = 0;
	while (current)
	{
		i++;
		current = current->next;
	}

	return i;
}

int process_inject_arguments(struct process *process, struct command_argument *root_argument)
{
	int res = 0;
	struct command_argument *current = root_argument;
	int i = 0;
	int argc = process_count_command_arguments(root_argument);
	if (argc == 0)
	{
		res = -EIO;
		goto out;
	}

	char **argv = process_malloc(process, sizeof(const char *) * argc);
	if (!argv)
	{
		res = -ENOMEM;
		goto out;
	}

	while (current)
	{
		char *argument_str = process_malloc(process, sizeof(current->argument));
		if (!argument_str)
		{
			res = -ENOMEM;
			goto out;
		}

		strncpy(argument_str, current->argument, sizeof(current->argument));
		argv[i] = argument_str;
		current = current->next;
		i++;
	}

	process->arguments.argc = argc;
	process->arguments.argv = argv;

out:
	return res;
}

void process_free(struct process *process, void *ptr)
{
	int res = 0;
	// unlink the pages from the process for given address
	struct process_allocation allocation;
	res = process_get_allocation_by_start_addr(process, ptr, &allocation);
	if (res < 0)
	{
		// oops, it's not our pointer
		return;
	}

	res = paging_map_to(process->paging_desc, allocation.ptr, allocation.ptr, paging_align_address(allocation.ptr + allocation.size), 0x00);
	if (res < 0)
	{
		return;
	}

	// unjoin allocation
	process_allocation_unjoin(process, ptr);

	// free process ptr memory
	kfree(ptr);
}

static int process_load_binary(const char *filename, struct process *process)
{
	int res = 0;
	void *program_data_ptr = 0;
	int fd = fopen(filename, "r");
	if (!fd)
	{
		res = -EIO;
		goto out;
	}

	struct file_stat stat;
	res = fstat(fd, &stat);
	if (res != MYOS_ALL_OK)
	{
		goto out;
	}

	program_data_ptr = kzalloc(stat.filesize);
	if (!program_data_ptr)
	{
		res = -ENOMEM;
		goto out;
	}

	if (fread(program_data_ptr, stat.filesize, 1, fd) != 1)
	{
		res = -EIO;
		goto out;
	}

	process->filetype = PROCESS_FILETYPE_BIN;
	process->ptr = program_data_ptr;
	process->size = stat.filesize;

out:
	if (res < 0)
	{
		if (program_data_ptr)
		{
			kfree(program_data_ptr);
		}
	}
	fclose(fd);
	return res;
}

static int process_load_elf(const char *filename, struct process *process)
{
	int res = 0;
	struct elf_file *elf_file = 0;
	res = elf_load(filename, &elf_file);
	if (ISERR(res))
	{
		goto out;
	}

	process->filetype = PROCESS_FILETYPE_ELF;
	process->elf_file = elf_file;

out:
	return res;
}

static int process_load_data(const char *filename, struct process *process)
{
	int res = 0;
	res = process_load_elf(filename, process);
	if (res == -EINFORMAT)
	{
		res = process_load_binary(filename, process);
	}
	return res;
}

int process_map_binary(struct process *process)
{
	int res = 0;
	paging_map_to(process->paging_desc, (void *)MYOS_PROGRAM_VIRTUAL_ADDRESS, process->ptr, paging_align_address(process->ptr + process->size), PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL | PAGING_IS_WRITEABLE);
	return res;
}

int process_map_elf(struct process *process)
{
	int res = 0;
	struct elf_file *elf_file = process->elf_file;
	struct elf_header *header = elf_header(elf_file);
	struct elf64_phdr *phdrs = elf_pheader(header);
	for (int i = 0; i < header->e_phnum; i++)
	{
		struct elf64_phdr *phdr = &phdrs[i];
		void *phdr_phys_address = elf_phdr_phys_address(elf_file, phdr);
		int flags = PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL;
		if (phdr->p_flags & PF_W)
		{
			flags |= PAGING_IS_WRITEABLE;
		}

		res = paging_map_to(process->paging_desc, paging_align_to_lower_page((void *)(uintptr_t)phdr->p_vaddr), paging_align_to_lower_page(phdr_phys_address), paging_align_address(phdr_phys_address + phdr->p_memsz), flags);
		if (ISERR(res))
		{
			break;
		}
	}

	return res;
}

int process_map_memory(struct process *process)
{
	int res = 0;

	// map all e820 memory regions
	paging_map_e820_memory_regions(process->paging_desc);

	switch (process->filetype)
	{
	case PROCESS_FILETYPE_ELF:
		res = process_map_elf(process);
		break;

	case PROCESS_FILETYPE_BIN:
		res = process_map_binary(process);
		break;

	default:
		panic("process_map_memory: Invalid filetype\n");
	}

	if (res < 0)
	{
		goto out;
	}

	// finally map the stack
	paging_map_to(process->paging_desc, (void *)MYOS_PROGRAM_VIRTUAL_STACK_ADDRESS_END, process->stack, paging_align_address(process->stack + MYOS_USER_PROGRAM_STACK_SIZE), PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL | PAGING_IS_WRITEABLE);
out:
	return res;
}

int process_get_free_slot()
{
	int res = 0;
	bool found = false;
	size_t total_process_slots = vector_count(process_vector);
	for (size_t i = 0; i < total_process_slots; i++)
	{
		struct process *process_out = NULL;
		res = vector_at(process_vector, i, &process_out, sizeof(process_out));
		if (res < 0)
		{
			break;
		}

		if (!process_out)
		{
			found = true;
			res = i;
			break;
		}
	}

	if (res < 0)
	{
		goto out;
	}

	if (!found)
	{
		struct process *null_process = NULL;
		int process_index = vector_push(process_vector, &null_process);
		res = process_index;
	}

out:
	return res;
}

int process_load(const char *filename, struct process **process)
{
	int res = 0;
	int process_slot = process_get_free_slot();
	if (process_slot < 0)
	{
		res = -EISTKN;
		goto out;
	}

	res = process_load_for_slot(filename, process, process_slot);

out:
	return res;
}

int process_load_switch(const char *filename, struct process **process)
{
	int res = process_load(filename, process);
	if (res == 0)
	{
		process_switch(*process);
	}

	return res;
}

int process_load_for_slot(const char *filename, struct process **process, int process_slot)
{
	int res = 0;
	struct process *_process;
	if (process_get(process_slot) != 0)
	{
		res = -EISTKN;
		goto out;
	}

	_process = kzalloc(sizeof(struct process));
	if (!_process)
	{
		res = -ENOMEM;
		goto out;
	}

	process_init(_process);
	res = process_load_data(filename, _process);
	if (res < 0)
	{
		goto out;
	}

	_process->stack = kzalloc(MYOS_USER_PROGRAM_STACK_SIZE);
	if (!_process->stack)
	{
		res = -ENOMEM;
		goto out;
	}

	strncpy(_process->filename, filename, sizeof(_process->filename));
	_process->id = process_slot;
	_process->paging_desc = paging_desc_new(PAGING_MAP_LEVEL_4);
	if (!_process->paging_desc)
	{
		res = -EIO;
		goto out;
	}

	res = process_map_memory(_process);
	if (res < 0)
	{
		goto out;
	}

	_process->task = task_new(_process);
	if (ERROR_I(_process->task) == 0)
	{
		res = ERROR_I(_process->task);
		_process->task = NULL;
		goto out;
	}

	*process = _process;

	// overwrite free process pointer with new process
	vector_overwrite(process_vector, process_slot, &_process, sizeof(&_process));

out:
	if (ISERR(res))
	{
		if (_process)
		{
			process_free_process(_process);
			_process = NULL;
			*process = NULL;
		}
		// TODO: free process data
	}
	return res;
}

bool process_is_stack_memory(struct process *process, void *addr)
{
	return (uintptr_t)addr >= MYOS_PROGRAM_VIRTUAL_STACK_ADDRESS_END && (uintptr_t)addr <= MYOS_PROGRAM_VIRTUAL_STACK_ADDRESS_START;
}

int process_get_allocation_by_addr(struct process *process, void *addr, struct process_allocation_request *allocation_request_out)
{
	// null the request out
	memset(allocation_request_out, 0, sizeof(struct process_allocation_request));

	// check if this is stack memory
	if (process_is_stack_memory(process, addr))
	{
		uint64_t addr_int = (uint64_t)addr;
		uint64_t stack_size = MYOS_USER_PROGRAM_STACK_SIZE;

		// start of stack is higher in memory
		uint64_t total_bytes_left = MYOS_PROGRAM_VIRTUAL_STACK_ADDRESS_START - addr_int;
		allocation_request_out->allocation.ptr = (void *)MYOS_PROGRAM_VIRTUAL_STACK_ADDRESS_END;
		allocation_request_out->allocation.end = (void *)MYOS_PROGRAM_VIRTUAL_STACK_ADDRESS_START;
		allocation_request_out->allocation.size = stack_size;
		allocation_request_out->flags |= PROCESS_ALLOCATION_REQUEST_IS_STACK_MEMORY;
		allocation_request_out->peek.addr = addr;
		allocation_request_out->peek.end = (void *)MYOS_PROGRAM_VIRTUAL_STACK_ADDRESS_START;
		allocation_request_out->peek.total_bytes_left = total_bytes_left;
		return 0;
	}

	// check the heap since it's not stack memory
	size_t total_allocations = vector_count(process->allocations);
	for (size_t i = 0; i < total_allocations; i++)
	{
		struct process_allocation allocation;
		int res = vector_at(process->allocations, i, &allocation, sizeof(allocation));
		if (res < 0)
		{
			break;
		}

		uint64_t allocation_addr = (uint64_t)allocation.ptr;
		uint64_t allocation_addr_end = (uint64_t)allocation.end;
		if ((uint64_t)addr >= allocation_addr && (uint64_t)addr <= allocation_addr_end)
		{
			size_t bytes_used = (uint64_t)addr - allocation_addr;
			size_t bytes_left = allocation_addr_end - bytes_used;
			allocation_request_out->allocation = allocation;
			allocation_request_out->peek.addr = addr;
			allocation_request_out->peek.end = (void *)allocation_addr_end;
			allocation_request_out->peek.total_bytes_left = bytes_left;
			return 0;
		}
	}

	return -EIO;
}

int process_validate_memory_or_terminate(struct process *process, void *virt_ptr, size_t space_needed)
{
	int res = 0;
	struct process_allocation_request allocation_request;
	res = process_get_allocation_by_addr(process, virt_ptr, &allocation_request);
	if (res < 0)
	{
		goto out;
	}

	if (allocation_request.peek.total_bytes_left < space_needed)
	{
		res = -EINVARG;
		goto out;
	}

out:
	if (res < 0)
	{
		process_terminate(process);
	}

	return res;
}

int process_fstat(struct process *process, int fd, struct file_stat *virt_filestat_addr)
{
	int res = 0;
	res = process_validate_memory_or_terminate(process, virt_filestat_addr, sizeof(*virt_filestat_addr));
	if (res < 0)
	{
		goto out;
	}

	struct file_stat *phys_filestat_addr = task_virtual_addr_to_phys(process->task, virt_filestat_addr);
	if (!phys_filestat_addr)
	{
		res = -EINVARG;
		goto out;
	}

	res = fstat(fd, phys_filestat_addr);
	if (res < 0)
	{
		goto out;
	}

out:
	return res;
}

int process_fseek(struct process *process, int fd, int offset, FILE_SEEK_MODE whence)
{
	int res = 0;
	struct process_file_handle *handle = process_file_handle_get(process, fd);
	if (!handle)
	{
		res = -EIO;
		goto out;
	}

	res = fseek(handle->fd, offset, whence);
	if (res < 0)
	{
		goto out;
	}

out:
	return res;
}
int process_fread(struct process *process, void *virt_ptr, uint64_t size, uint64_t nmemb, int fd)
{
	int res = 0;
	struct process_file_handle *handle = process_file_handle_get(process, fd);
	if (!handle)
	{
		res = -EINVARG;
		goto out;
	}

	size_t true_size = size * nmemb;
	res = process_validate_memory_or_terminate(process, virt_ptr, true_size);
	if (res < 0)
	{
		goto out;
	}

	void *phys_ptr = task_virtual_addr_to_phys(process->task, virt_ptr);
	if (!phys_ptr)
	{
		res = -EIO;
		goto out;
	}

	res = fread(phys_ptr, size, nmemb, handle->fd);
	if (res < 0)
	{
		goto out;
	}

out:
	return res;
}

int process_fclose(struct process *process, int fd)
{
	int res = 0;
	struct process_file_handle *handle = process_file_handle_get(process, fd);
	if (!handle)
	{
		res = -EINVARG;
		goto out;
	}

	fclose(handle->fd);

	// remove from vector
	vector_pop_element(process->file_handles, &handle, sizeof(handle));
	kfree(handle);
out:
	return res;
}

int process_fopen(struct process *process, const char *path, const char *mode)
{
	int res = 0;
	int fd = fopen(path, mode);
	if (fd <= 0)
	{
		res = -EIO;
		goto out;
	}

	res = fd;

	// allocate memory for process_file_handle
	struct process_file_handle *handle = kzalloc(sizeof(struct process_file_handle *));
	if (!handle)
	{
		res = -ENOMEM;
		goto out;
	}

	handle->fd = fd;
	strncpy(handle->file_path, path, sizeof(handle->file_path));
	strncpy(handle->mode, mode, sizeof(handle->mode));

	// add to process file_handles vector
	vector_push(process->file_handles, &handle);

out:
	if (res < 0)
	{
		fclose(fd);
	}
	return res;
}

struct process_file_handle *process_file_handle_get(struct process *process, int fd)
{
	size_t total_handles = vector_count(process->file_handles);
	for (size_t i = 0; i < total_handles; i++)
	{
		struct process_file_handle *handle = NULL;
		vector_at(process->file_handles, i, &handle, sizeof(handle));
		if (handle && handle->fd == fd)
		{
			return handle;
		}
	}

	return NULL;
}

int process_close_file_handles(struct process *process)
{
	int res = 0;
	size_t total_handles = vector_count(process->file_handles);
	for (size_t i = 0; i < total_handles; i++)
	{
		struct process_file_handle *handle = NULL;
		vector_at(process->file_handles, i, &handle, sizeof(handle));
		if (handle)
		{
			fclose(handle->fd);
			kfree(handle);
		}
	}

	vector_free(process->file_handles);
	process->file_handles = NULL;
	return res;
}