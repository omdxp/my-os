#pragma once

#include <stdint.h>
#include "task.h"
#include "config.h"
#include <stddef.h>
#include <stdbool.h>

#define PROCESS_FILETYPE_ELF 0
#define PROCESS_FILETYPE_BIN 1
typedef unsigned char PROCESS_FILETYPE;

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

struct process
{
	// process ID
	uint16_t id;

	char filename[MYOS_MAX_PATH];

	// main process task
	struct task *task;

	// process memory allocations (malloc)
	struct vector *allocations; // vector of struct process_allocation

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

	// process arguments
	struct process_arguments arguments;
};

int process_switch(struct process *process);
int process_load_switch(const char *filename, struct process **process);
int process_load_for_slot(const char *filename, struct process **process, int process_slot);
int process_load(const char *filename, struct process **process);
struct process *process_current();
struct process *process_get(int process_id);
void *process_malloc(struct process *process, size_t size);
void process_free(struct process *process, void *ptr);
void process_get_arguments(struct process *process, int *argc, char ***argv);
int process_inject_arguments(struct process *process, struct command_argument *root_argument);
int process_terminate(struct process *process);

struct process_file_handle *process_file_handle_get(struct process *process, int fd);
int process_fopen(struct process *process, const char *path, const char *mode);
int process_fclose(struct process *process, int fd);
int process_fread(struct process *process, void *virt_ptr, uint64_t size, uint64_t nmemb, int fd);