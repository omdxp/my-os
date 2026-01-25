#pragma once

#include <stddef.h>
#include <stdbool.h>

struct file_stat;
struct window;

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

void print(const char *filename);
int myos_getkey();
void *myos_malloc(size_t size);
void *myos_realloc(void *old_ptr, size_t new_size);
void myos_free(void *ptr);
void myos_putchar(char c);
int myos_getkeyblock();
void myos_terminal_readline(char *out, int max, bool output_while_typing);
void myos_process_load_start(const char *filename);
struct command_argument *myos_parse_command(const char *command, int max);
void myos_process_get_arguments(struct process_arguments *arguments);
int myos_system(struct command_argument *arguments);
int myos_system_run(const char *command);
void myos_exit();
int myos_fopen(const char *filename, const char *mode);
void myos_fclose(int fd);
long myos_fread(void *buffer, size_t size, size_t count, long fd);
long myos_fseek(long fd, long offset, long whence);
long myos_fstat(long fd, struct file_stat *filestat_out);
void *myos_window_create(const char *title, long width, long height, long flags, long id);
void myos_divert_stdout_to_window(struct window *win);