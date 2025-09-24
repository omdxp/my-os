#include "idt.h"
#include "config.h"
#include "kernel.h"
#include "memory/memory.h"
#include "io/io.h"
#include "task/task.h"
#include "memory/heap/kheap.h"
#include "status.h"
#include "task/process.h"

struct idt_desc idt_descriptors[MYOS_TOTAL_INTERRUPTS];
struct idtr_desc idtr_descriptor;

extern void *interrupt_pointer_table[MYOS_TOTAL_INTERRUPTS];

static INTERRUPT_CALLBACK_FUNCTION interrupt_callbacks[MYOS_TOTAL_INTERRUPTS];

static ISR80H_COMMAND isr80h_commands[MYOS_MAX_ISR80H_COMMANDS];

extern void idt_load(struct idtr_desc *ptr);
extern void no_interrupt();
extern void isr80h_wrapper();

void no_interrupt_handler()
{
	outb(0x20, 0x20);
}

void interrupt_handler(int interrupt, struct interrupt_frame *frame)
{
	kernel_page();
	if (interrupt_callbacks[interrupt] != 0)
	{
		task_current_save_state(frame);
		interrupt_callbacks[interrupt](frame);
	}

	task_page();
	outb(0x20, 0x20);
}

void idt_zero()
{
	print("Divide by zero error\n");
}

void idt_set(int interrupt_no, void *address)
{
	struct idt_desc *desc = &idt_descriptors[interrupt_no];
	uintptr_t _address = (uintptr_t)address;
	desc->offset_1 = _address & 0x000000000000ffff;
	desc->selector = KERNEL_LONG_MODE_CODE_SELECTOR;
	desc->ist = 0;

	desc->type_attr = 0xee; // interrupt gate
	if (interrupt_no <= 0x31)
	{
		desc->type_attr = 0x8e; // interrupt gate, present
	}

	desc->offset_2 = (_address >> 16) & 0x000000000000ffff;
	desc->offset_3 = (_address >> 32) & 0x00000000ffffffff;
}

void idt_handle_exception()
{
	process_terminate(task_current()->process);
	task_next();
}

void idt_clock()
{
	outb(0x20, 0x20);

	// switch to the next task
	task_next();
}

void idt_init()
{
	memset(idt_descriptors, 0, sizeof(idt_descriptors));
	idtr_descriptor.limit = sizeof(idt_descriptors) - 1;
	idtr_descriptor.base = (uint64_t)idt_descriptors;

	for (int i = 0; i < MYOS_TOTAL_INTERRUPTS; i++)
	{
		idt_set(i, interrupt_pointer_table[i]);
	}

	idt_set(0, idt_zero);
	idt_set(0x80, isr80h_wrapper);

	for (int i = 0; i < 0x20; i++)
	{
		idt_register_interrupt_callback(i, idt_handle_exception);
	}

	idt_register_interrupt_callback(0x20, idt_clock); // 0x20 timer interrupt

	// load interrupt descriptor table
	idt_load(&idtr_descriptor);
}

int idt_register_interrupt_callback(int interrupt, INTERRUPT_CALLBACK_FUNCTION interrupt_callback)
{
	if (interrupt < 0 || interrupt >= MYOS_TOTAL_INTERRUPTS)
	{
		return -EINVARG;
	}

	interrupt_callbacks[interrupt] = interrupt_callback;
	return 0;
}

void isr80h_register_command(int command_id, ISR80H_COMMAND command)
{
	if (command_id < 0 || command_id >= MYOS_MAX_ISR80H_COMMANDS)
	{
		panic("isr80h_register_command: Command is out of bounds\n");
	}

	if (isr80h_commands[command_id])
	{
		panic("isr80h_register_command: Attempt to overwrite an existing command\n");
	}

	isr80h_commands[command_id] = command;
}

void *isr80h_handle_command(int command, struct interrupt_frame *frame)
{
	void *res = 0;
	if (command < 0 || command >= MYOS_MAX_ISR80H_COMMANDS)
	{
		// invalid command
		return 0;
	}

	ISR80H_COMMAND command_func = isr80h_commands[command];
	if (!command_func)
	{
		return 0;
	}

	res = command_func(frame);
	return res;
}

void *isr80h_handler(int command, struct interrupt_frame *frame)
{
	void *res = 0;
	kernel_page();
	task_current_save_state(frame);
	res = isr80h_handle_command(command, frame);
	task_page();
	return res;
}
