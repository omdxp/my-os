#pragma once

#include "graphics/terminal.h"
#include "graphics/graphics.h"
#include "config.h"
#include "lib/vector.h"
#include "memory/heap/kheap.h"
#include "keyboard/keyboard.h"
#include "memory/memory.h"
#include "string/string.h"
#include "graphics/font.h"
#include "task/process.h"
#include "status.h"
#include "kernel.h"
#include <stddef.h>
#include <stdint.h>

struct window;

enum
{
	WINDOW_EVENT_TYPE_NULL,			// no event
	WINDOW_EVENT_TYPE_FOCUS,		// window focus event
	WINDOW_EVENT_TYPE_LOST_FOCUS,	// window lost focus event
	WINDOW_EVENT_TYPE_MOUSE_MOVE,	// mouse move event
	WINDOW_EVENT_TYPE_MOUSE_CLICK,	// mouse click event
	WINDOW_EVENT_TYPE_WINDOW_CLOSE, // window close event
	WINDOW_EVENT_TYPE_KEYPRESS,		// keypress event

};

struct window_event
{
	int type;			   // type of the event
	int win_id;			   // id of the window generating the event
	struct window *window; // pointer to the window generating the event
	union
	{
		struct
		{
			// no additional data for close event
		} focus; // focus event data

		struct
		{
			int x; // x position of the mouse click
			int y; // y position of the mouse click
		} move;	   // mouse move event data

		struct
		{
			int x; // x position of the mouse click
			int y; // y position of the mouse click
		} click;   // mouse click event data

		struct
		{
			char key; // key that was pressed
		} keypress;	  // keypress event data
	} data;			  // event-specific data
};

typedef int (*WINDOW_EVENT_HANDLER)(struct window *window, struct window_event *event);

enum
{
	WINDOW_FLAG_BORDERLESS = 0x1,			  // Window has no border
	WINDOW_FLAG_CLICK_THROUGH = 0x2,		  // Window ignores mouse clicks
	WINDOW_FLAG_BACKGROUND_TRANSPARENT = 0x4, // Window background is transparent
};

struct window
{
	int id;								 // unique identifier for the window
	struct terminal *title_bar_terminal; // title bar terminal
	struct terminal *terminal;			 // body terminal

	struct graphics_info *root_graphics;	  // pointer to the root graphics_info (contains sub-graphics for the window)
	struct graphics_info *title_bar_graphics; // pointer to the title bar graphics_info
	struct graphics_info *graphics;			  // pointer to the body graphics_info

	struct
	{
		struct
		{
			size_t x;
			size_t y;
			size_t width;
			size_t height;
		} close_button; // close button dimensions and position

	} title_bar_components; // components specific to the title bar

	struct
	{
		struct vector *handlers; // vector of WINDOW_EVENT_HANDLER*
	} event_handlers;			 // event handlers for the window

	size_t width;  // width of the window
	size_t height; // height of the window

	size_t x; // x position of the window
	size_t y; // y position of the window

	size_t z_index; // z-index of the window

	char title[WINDOW_MAX_TITLE_LENGTH]; // title of the window
	int flags;							 // flags for window behavior
};

int window_system_initialize();
int window_system_initialize_stage2();
void window_set_z_index(struct window *window, int z_index);
void window_unfocus(struct window *window);
void window_focus(struct window *window);
struct window *
window_create(struct graphics_info *graphics_info, struct font *font, const char *title, size_t x, size_t y, size_t width, size_t height, int flags, int id);
void window_event_handler_register(struct window *window, WINDOW_EVENT_HANDLER handler);
void window_event_handler_unregister(struct window *window, WINDOW_EVENT_HANDLER handler);
int window_position_set(struct window *window, size_t new_x, size_t new_y);
void window_redraw(struct window *window);
struct terminal *window_terminal(struct window *window);