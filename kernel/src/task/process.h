#pragma once

#include <stdint.h>
#include "task.h"
#include "config.h"
#include "fs/file.h"
#include <stddef.h>
#include <stdbool.h>

#define PROCESS_FILETYPE_ELF 0
#define PROCESS_FILETYPE_BIN 1
#define PROCESS_MAX_WINDOW_RECORDED 1000
typedef unsigned char PROCESS_FILETYPE;

struct window;
struct graphics_info;
struct window_event;
struct framebuffer_pixel;

struct command_argument
{
	char argument[512];
	struct command_argument *next;
};

struct process_arguments
{
	int argc;
	char **argv;
};

struct process_allocation
{
	void *ptr;
	void *end;
	size_t size;
};

enum
{
	PROCESS_ALLOCATION_REQUEST_IS_STACK_MEMORY = 0x01,
};

struct process_allocation_request
{
	struct process_allocation allocation;
	int flags;
	struct
	{
		void *addr;
		void *end;
		size_t total_bytes_left;
	} peek;
};

struct process_file_handle
{
	int fd;						   // file descriptor
	char file_path[MYOS_MAX_PATH]; // path to the file
	char mode[2];				   // file mode (e.g., "r", "w", etc.)
};

struct process_userspace_window
{
	char title[WINDOW_MAX_TITLE_LENGTH];
	int width;
	int height;
};

struct process_window
{
	struct process_userspace_window *user_win;
	struct window *kernel_win;
};

struct process
{
	// process ID
	uint16_t id;

	char filename[MYOS_MAX_PATH];

	// main process task
	struct task *task;

	// page directory of process virtual memory
	struct paging_desc *paging_desc;

	// process memory allocations (malloc)
	struct vector *allocations; // vector of struct process_allocation

	// vector of struct userland_ptr*
	struct vector *kernel_userland_ptrs_vector;

	// vector of struct process_file_handle*
	struct vector *file_handles;

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

	// vector of struct process_window*
	struct vector *windows;

	// window events
	struct
	{
		struct vector *events; // vector of struct window_event
		size_t index;		   // current index in the events vector
		size_t total_unpopped; // total unpopped events
	} window_events;

	// process arguments
	struct process_arguments arguments;

	// special system output window
	struct process_window *sysout_win;
};

void process_system_init();

int process_switch(struct process *process);
int process_load_switch(const char *filename, struct process **process);
int process_load_for_slot(const char *filename, struct process **process, int process_slot);
int process_load(const char *filename, struct process **process);
struct process *process_current();
struct process *process_get(int process_id);
void *process_malloc(struct process *process, size_t size);
void *process_realloc(struct process *process, void *old_virt_ptr, size_t new_size);
void process_free(struct process *process, void *ptr);
void process_get_arguments(struct process *process, int *argc, char ***argv);
int process_inject_arguments(struct process *process, struct command_argument *root_argument);
int process_terminate(struct process *process);

struct process_file_handle *process_file_handle_get(struct process *process, int fd);
int process_fopen(struct process *process, const char *path, const char *mode);
int process_fclose(struct process *process, int fd);
int process_fread(struct process *process, void *virt_ptr, uint64_t size, uint64_t nmemb, int fd);
int process_fseek(struct process *process, int fd, int offset, FILE_SEEK_MODE whence);
int process_fstat(struct process *process, int fd, struct file_stat *virt_filestat_addr);
struct process_window *process_window_create(struct process *process, char *title, int width, int height, int flags, int id);
bool process_owns_kernel_window(struct process *process, struct window *kernel_window);
struct process *process_get_from_kernel_window(struct window *kernel_window);
struct process_window *process_window_get_from_user_window(struct process *process, struct process_userspace_window *user_win);
void process_close_windows(struct process *process);
void process_print(struct process *process, const char *str);
void process_print_char(struct process *process, char c);
void process_set_sysout_window(struct process *process, struct process_window *proc_win);
int process_push_window_event(struct process *process, struct window_event *event);
int process_pop_window_event(struct process *process, struct window_event *event_out);
void process_windows_closed(struct process *process, struct process_window *proc_win);
int process_map_graphics_framebuffer_pixels_into_userspace(struct process *process, struct graphics_info *graphics_in, struct framebuffer_pixel **virt_addr_out, size_t *size_out);
int process_map_into_userspace(struct process *process, void *phys_ptr, size_t t_size, int map_flags, void **virt_addr_out);