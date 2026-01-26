#include "keyboard.h"
#include "status.h"
#include "kernel.h"
#include "memory/heap/kheap.h"
#include "memory/memory.h"
#include "task/process.h"
#include "task/task.h"
#include "ps2.h"

static struct keyboard *keyboard_list_head = 0;
static struct keyboard *keyboard_list_last = 0;

void keyboard_init()
{
	keyboard_insert(ps2_init());
}

int keyboard_insert(struct keyboard *keyboard)
{
	int res = 0;
	if (keyboard->init == 0)
	{
		res = -EINVARG;
		goto out;
	}

	if (keyboard_list_last)
	{
		keyboard_list_last->next = keyboard;
		keyboard_list_last = keyboard;
	}
	else
	{
		keyboard_list_head = keyboard;
		keyboard_list_last = keyboard;
	}

	res = keyboard->init();
	if (res >= 0)
	{
		if (keyboard->key_listeners != NULL)
		{
			panic("keyboard_insert: keyboard->key_listeners is already initialized");
		}

		keyboard->key_listeners = vector_new(sizeof(struct keyboard_listener *), 4, 0);
	}
out:
	return res;
}

static int keyboard_get_tail_index(struct process *process)
{
	return process->keyboard.tail % sizeof(process->keyboard.buffer);
}

void keyboard_backspace(struct process *process)
{
	process->keyboard.tail -= 1;
	int real_index = keyboard_get_tail_index(process);
	process->keyboard.buffer[real_index] = 0x00;
}

void keyboard_set_capslock(struct keyboard *keyboard, KEYBOARD_CAPSLOCK_STATE state)
{
	keyboard->capslock_state = state;
}

KEYBOARD_CAPSLOCK_STATE keyboard_get_capslock(struct keyboard *keyboard)
{
	return keyboard->capslock_state;
}

void keyboard_push(char c)
{
	struct process *process = process_current();
	if (!process)
	{
		return;
	}

	if (c == 0)
	{
		return;
	}

	int real_index = keyboard_get_tail_index(process);
	process->keyboard.buffer[real_index] = c;
	process->keyboard.tail++;

	struct keyboard_event event = {0};
	event.type = KEYBOARD_EVENT_KEYPRESS;
	event.data.keypress.key = (int)c;

	keyboard_push_event_to_listeners(keyboard_default(), &event);
}

char keyboard_pop()
{
	if (!task_current())
	{
		return 0;
	}

	struct process *process = task_current()->process;
	int real_index = process->keyboard.head % sizeof(process->keyboard.buffer);
	char c = process->keyboard.buffer[real_index];
	if (c == 0x00)
	{
		// nothing to pop
		return 0;
	}

	process->keyboard.buffer[real_index] = 0;
	process->keyboard.head++;
	return c;
}

struct keyboard *keyboard_default()
{
	return keyboard_list_head;
}

int keyboard_register_handler(struct keyboard *keyboard, struct keyboard_listener keyboard_listener)
{
	int res = 0;
	struct keyboard_listener *listener_clone = NULL;
	if (!keyboard)
	{
		keyboard = keyboard_default();
	}

	if (!keyboard)
	{
		res = -EINVARG;
		goto out;
	}

	listener_clone = kzalloc(sizeof(struct keyboard_listener));
	if (!listener_clone)
	{
		res = -ENOMEM;
		goto out;
	}

	memcpy(listener_clone, &keyboard_listener, sizeof(struct keyboard_listener));

	vector_push(keyboard->key_listeners, &listener_clone);

out:
	return res;
}

void keyboard_push_event_to_listeners(struct keyboard *keyboard, struct keyboard_event *event)
{
	size_t total_listeners = vector_count(keyboard->key_listeners);
	for (size_t i = 0; i < total_listeners; i++)
	{
		struct keyboard_listener *current_listener = NULL;
		vector_at(keyboard->key_listeners, i, &current_listener, sizeof(struct keyboard_listener *));
		if (current_listener && current_listener->on_event)
		{
			current_listener->on_event(keyboard, event);
		}
	}
}

int keyboard_unregister_handler(struct keyboard *keyboard, struct keyboard_listener keyboard_listener)
{
	int res = 0;
	struct keyboard_listener *keyboard_listener_ptr = keyboard_get_listener_ptr(keyboard, keyboard_listener);
	if (keyboard_listener_ptr)
	{
		vector_pop_element(keyboard->key_listeners, &keyboard_listener_ptr, sizeof(struct keyboard_listener *));
	}

	kfree(keyboard_listener_ptr);
	return res;
}

struct keyboard_listener *keyboard_get_listener_ptr(struct keyboard *keyboard, struct keyboard_listener keyboard_listener)
{
	struct keyboard_listener *listener_heap_ptr = NULL;
	size_t total_children = vector_count(keyboard->key_listeners);
	for (size_t i = 0; i < total_children; i++)
	{
		struct keyboard_listener *current_listener = NULL;
		vector_at(keyboard->key_listeners, i, &current_listener, sizeof(struct keyboard_listener *));
		if (current_listener && keyboard_listener.on_event && current_listener->on_event == keyboard_listener.on_event)
		{
			listener_heap_ptr = current_listener;
			break;
		}
	}

	return listener_heap_ptr;
}