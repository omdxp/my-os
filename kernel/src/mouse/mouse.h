#pragma once

#include "lib/vector.h"
#include "graphics/graphics.h"
#include "graphics/window.h"
#include "kernel.h"
#include "status.h"

#define MOUSE_GRAPHIC_DEFAULT_WIDTH 10
#define MOUSE_GRAPHIC_DEFAULT_HEIGHT 10
#define MOUSE_GRAPHIC_Z_INDEX 100000

enum
{
	MOUSE_NO_CLICK,
	MOUSE_LEFT_BUTTON_CLICK,
	MOUSE_RIGHT_BUTTON_CLICK,
	MOUSE_MIDDLE_BUTTON_CLICK,
};

typedef int MOUSE_CLICK_TYPE;

struct mouse;
typedef int (*MOUSE_INIT_FUNCTION)(struct mouse *mouse);
typedef void (*MOUSE_DRAW_FUNCTION)(struct mouse *mouse);

typedef void (*MOUSE_CLICK_EVENT_HANDLER_FUNCTION)(struct mouse *mouse, int clicked_x, int clicked_y, MOUSE_CLICK_TYPE click_type);
typedef void (*MOUSE_MOVE_EVENT_HANDLER_FUNCTION)(struct mouse *mouse, int moved_to_x, int moved_to_y);

struct mouse
{
	MOUSE_INIT_FUNCTION init; // function to initialize the mouse
	MOUSE_DRAW_FUNCTION draw; // function to draw the mouse
	char name[32];			  // name of the mouse

	struct
	{
		int x;
		int y;
	} coords; // current coordinates of the mouse

	struct
	{
		struct window *window;
		int width;
		int height;
	} graphic; // graphic of the mouse

	struct
	{
		struct vector *click_handlers; // vector of MOUSE_CLICK_EVENT_HANDLER_FUNCTION
		struct vector *move_handlers;  // vector of MOUSE_MOVE_EVENT_HANDLER_FUNCTION
	} event_handlers;				   // event handlers for the mouse

	void *private; // private data for the mouse
};

int mouse_system_load_static_drivers();
int mouse_system_init();
void mouse_draw_default_impl(struct mouse *mouse);
int mouse_register(struct mouse *mouse);
void mouse_position_set(struct mouse *mouse, size_t x, size_t y);
void mouse_click(struct mouse *mouse, MOUSE_CLICK_TYPE type);
void mouse_move(struct mouse *mouse);
void mouse_unregister_move_handler(struct mouse *mouse, MOUSE_MOVE_EVENT_HANDLER_FUNCTION move_handler);
void mouse_unregister_click_handler(struct mouse *mouse, MOUSE_CLICK_EVENT_HANDLER_FUNCTION click_handler);
void mouse_register_move_handler(struct mouse *mouse, MOUSE_MOVE_EVENT_HANDLER_FUNCTION move_handler);
void mouse_register_click_handler(struct mouse *mouse, MOUSE_CLICK_EVENT_HANDLER_FUNCTION click_handler);
void mouse_draw(struct mouse *mouse);