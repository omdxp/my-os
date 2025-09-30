#include "misc.h"
#include "idt/idt.h"
#include "task/task.h"

void *isr80h_comman0_sum(struct interrupt_frame *frame)
{
	intptr_t v2 = (uintptr_t)task_get_stack_item(task_current(), 1);
	intptr_t v1 = (uintptr_t)task_get_stack_item(task_current(), 0);
	return (void *)(v1 + v2);
}
