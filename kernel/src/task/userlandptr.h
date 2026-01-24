#pragma once

#include <stdbool.h>
#include <stdint.h>

struct userland_ptr
{
	void *kernel_ptr;
};

struct process;
struct userland_ptr *process_userland_pointer_create(struct process *process, void *kernel_ptr);
bool process_userland_pointer_registered(struct process *process, void *userland_ptr);
void process_userland_pointer_release(struct process *process, void *userland_ptr);
void *process_userland_pointer_kernel_ptr(struct process *process, void *userland_ptr);