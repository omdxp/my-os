#pragma once

#include "lib/vector.h"

#define KEYBOARD_CAPSLOCK_ON 1
#define KEYBOARD_CAPSLOCK_OFF 0

typedef int KEYBOARD_CAPSLOCK_STATE;

typedef int KEYBOARD_EVENT_TYPE;

enum
{
	KEYBOARD_EVENT_INVALID,
	KEYBOARD_EVENT_KEYPRESS,
	KEYBOARD_EVENT_CAPSLOCK_CHANGE,
};

struct keyboard;
struct keyboard_event
{
	KEYBOARD_EVENT_TYPE type;
	union
	{
		struct
		{
			int key;
		} keypress;

		struct
		{
			KEYBOARD_CAPSLOCK_STATE state;
		} capslock;
	} data;
	struct keyboard *keyboard;
};

typedef void (*KEYBOARD_EVENT_LISTENER_ON_EVENT)(struct keyboard *keyboard, struct keyboard_event *event);

struct keyboard_listener
{
	KEYBOARD_EVENT_LISTENER_ON_EVENT on_event;
};

struct process;

typedef int (*KEYBOARD_INIT_FUNCTION)();

struct keyboard
{
	KEYBOARD_INIT_FUNCTION init;
	char name[20];

	KEYBOARD_CAPSLOCK_STATE capslock_state;

	struct vector *key_listeners; // vector of struct keyboard_listener*

	struct keyboard *next;
};

void keyboard_init();
int keyboard_insert(struct keyboard *keyboard);
void keyboard_backspace(struct process *process);
void keyboard_push(char c);
char keyboard_pop();
void keyboard_set_capslock(struct keyboard *keyboard, KEYBOARD_CAPSLOCK_STATE state);
KEYBOARD_CAPSLOCK_STATE keyboard_get_capslock(struct keyboard *keyboard);
int keyboard_register_handler(struct keyboard *keyboard, struct keyboard_listener keyboard_listener);
struct keyboard *keyboard_default();
struct keyboard_listener *keyboard_get_listener_ptr(struct keyboard *keyboard, struct keyboard_listener keyboard_listener);
int keyboard_unregister_handler(struct keyboard *keyboard, struct keyboard_listener keyboard_listener);
void keyboard_push_event_to_listeners(struct keyboard *keyboard, struct keyboard_event *event);