#include "userlandptr.h"
#include "memory/heap/kheap.h"
#include "lib/vector.h"
#include "task/process.h"

struct userland_ptr *process_userland_pointer_create(struct process *process, void *kernel_ptr)
{
	struct userland_ptr *userland_ptr = kzalloc(sizeof(struct userland_ptr));
	if (!userland_ptr)
	{
		return NULL;
	}

	userland_ptr->kernel_ptr = kernel_ptr;
	vector_push(process->kernel_userland_ptrs_vector, &userland_ptr);
	return userland_ptr;
}

bool process_userland_pointer_registered(struct process *process, void *userland_ptr)
{
	size_t total_userland_ptrs = vector_count(process->kernel_userland_ptrs_vector);
	for (size_t i = 0; i < total_userland_ptrs; i++)
	{
		struct userland_ptr *current_ptr = NULL;
		vector_at(process->kernel_userland_ptrs_vector, i, &current_ptr, sizeof(struct userland_ptr *));
		if (current_ptr && current_ptr == userland_ptr)
		{
			return true;
		}
	}

	return false;
}

void process_userland_pointer_release(struct process *process, void *userland_ptr)
{
	if (!process_userland_pointer_registered(process, userland_ptr))
	{
		return;
	}

	vector_pop_element(process->kernel_userland_ptrs_vector, &userland_ptr, sizeof(struct userland_ptr *));
	kfree(userland_ptr);
}

void *process_userland_pointer_kernel_ptr(struct process *process, void *userland_ptr)
{
	if (!process_userland_pointer_registered(process, userland_ptr))
	{
		return NULL;
	}

	struct userland_ptr *ptr = (struct userland_ptr *)userland_ptr;
	return ptr->kernel_ptr;
}