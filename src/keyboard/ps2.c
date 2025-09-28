#include "ps2.h"
#include "keyboard.h"
#include "io/io.h"
#include <stdint.h>
#include <stddef.h>
#include "kernel.h"
#include "idt/idt.h"
#include "task/task.h"
#include "idt/irq.h"

#define PS2_KEYBOARD_CAPSLOCK 0x3a

int ps2_keyboard_init();
void ps2_keyboard_handle_interrupt();

static uint8_t keyboard_scan_set_one[] = {
	0x00, 0x1b, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
	'-', '=', 0x08, '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
	'O', 'P', '[', ']', 0x0d, 0x00, 'A', 'S', 'D', 'F', 'G', 'H',
	'J', 'K', 'L', ';', '\'', '`', 0x00, '\\', 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', ',', '.', '/', 0x00, '*', 0x00, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, '7', '8',
	'9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'};

struct keyboard ps2_keyboard = {
	.name = {"PS2"},
	.init = ps2_keyboard_init};

int ps2_keyboard_init()
{
	idt_register_interrupt_callback(ISR_KEYBOARD_INTERRUPT, ps2_keyboard_handle_interrupt);
	keyboard_set_capslock(&ps2_keyboard, KEYBOARD_CAPSLOCK_OFF);

	// enable the PS/2 keyboard's first port (the keyboard port)
	irq_enable(IRQ_KEYBOARD);

	outb(PS2_PORT, PS2_COMMAND_ENABLE_FIRST_PORT);
	return 0;
}

uint8_t ps2_keyboard_scancode_to_char(uint8_t scancode)
{
	size_t size_of_keyboard_set_one = sizeof(keyboard_scan_set_one) / sizeof(uint8_t);
	if (scancode > size_of_keyboard_set_one)
	{
		return 0;
	}

	char c = keyboard_scan_set_one[scancode];
	if (keyboard_get_capslock(&ps2_keyboard) == KEYBOARD_CAPSLOCK_OFF)
	{
		if (c >= 'A' && c <= 'Z')
		{
			c += 32;
		}
	}
	return c;
}

void ps2_keyboard_handle_interrupt()
{
	kernel_page();
	uint8_t scancode = insb(KEYBOARD_INPUT_PORT);
	insb(KEYBOARD_INPUT_PORT);

	if (scancode & PS2_KEYBOARD_KEY_RELEASED)
	{
		return;
	}

	if (scancode == PS2_KEYBOARD_CAPSLOCK)
	{
		KEYBOARD_CAPSLOCK_STATE old_state = keyboard_get_capslock(&ps2_keyboard);
		keyboard_set_capslock(&ps2_keyboard, old_state == KEYBOARD_CAPSLOCK_ON ? KEYBOARD_CAPSLOCK_OFF : KEYBOARD_CAPSLOCK_ON);
	}

	uint8_t c = ps2_keyboard_scancode_to_char(scancode);
	if (c != 0)
	{
		keyboard_push(c);
	}

	task_page();
}

struct keyboard *ps2_init()
{
	return &ps2_keyboard;
}
