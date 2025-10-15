#pragma once

#include "lib/vector.h"
#include "graphics/image/image.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

enum
{
	GRAPHICS_FLAG_ALLOW_OUT_OF_BOUNDS = 0b00000001,
	GRAPHICS_FLAG_CLONED_FRAMEBUFFER = 0b00000010,
	GRAPHICS_FLAG_CLONED_CHILDREN = 0b00000100,
	GRAPHICS_FLAG_DO_NOT_COPY_PIXELS = 0b00001000,
	GRAPHICS_FLAG_DO_NOT_OVERWRITE_TRANSPARENT_PIXELS = 0b00010000,

};

struct graphics_info;
typedef void (*GRAPHICS_MOUSE_CLICK_FUNCTION)(struct graphics_info *graphics, size_t rel_x, size_t rel_y, int type);
typedef void (*GRAPHICS_MOUSE_MOVE_FUNCTION)(struct graphics_info *graphics, size_t rel_x, size_t rel_y, size_t abs_x, size_t abs_y);

struct framebuffer_pixel
{
	uint8_t blue;
	uint8_t green;
	uint8_t red;
	uint8_t reserved;
};

struct graphics_info
{
	struct framebuffer_pixel *framebuffer; // pointer to the framebuffer
	uint32_t horizontal_resolution;		   // horizontal resolution in pixels
	uint32_t vertical_resolution;		   // vertical resolution in pixels
	uint32_t pixels_per_scanline;		   // number of pixels per scanline

	struct framebuffer_pixel *pixels; // pointer to the pixel array

	uint32_t width;	 // width of the framebuffer in pixels
	uint32_t height; // height of the framebuffer in pixels

	// absolute coordinates (in the entire screen)
	uint32_t starting_x; // starting x coordinate for drawing
	uint32_t starting_y; // starting y coordinate for drawing

	// relative coordinates (in the current drawing area)
	uint32_t relative_x; // relative x coordinate for drawing
	uint32_t relative_y; // relative y coordinate for drawing

	struct graphics_info *parent; // pointer to the parent graphics_info (if any)
	struct vector *children;	  // vector of child graphics_info (if any)

	uint32_t flags;	  // flags for additional information
	uint32_t z_index; // z-index for layering

	struct framebuffer_pixel ignore_color;	   // color to ignore (for transparency)
	struct framebuffer_pixel transparency_key; // color to treat as transparent

	struct
	{
		GRAPHICS_MOUSE_CLICK_FUNCTION mouse_click; // function pointer for mouse click event
		GRAPHICS_MOUSE_MOVE_FUNCTION mouse_move;   // function pointer for mouse move event
	} event_handlers;							   // event handlers for mouse events
};

void graphics_redraw(struct graphics_info *graphics_info);
void graphics_redraw_all();
void graphics_draw_pixel(struct graphics_info *graphics_info, uint32_t x, uint32_t y, struct framebuffer_pixel pixel);
void graphics_draw_image(struct graphics_info *graphics_info, struct image *image, int x, int y);
struct graphics_info *graphics_screen_info();
void graphics_setup(struct graphics_info *main_graphics_info);
void graphics_redraw_region(struct graphics_info *graphics_info, uint32_t local_x, uint32_t local_y, uint32_t width, uint32_t height);
void graphics_redraw_graphics_to_screen(struct graphics_info *relative_graphics, uint32_t rel_x, uint32_t rel_y, uint32_t width, uint32_t height);
void graphics_ignore_color(struct graphics_info *graphics_info, struct framebuffer_pixel pixel_color);
void graphics_transparency_key_set(struct graphics_info *graphics_info, struct framebuffer_pixel pixel_color);
void graphics_transparency_key_remove(struct graphics_info *graphics_info);
void graphics_ignore_color_finish(struct graphics_info *graphics_info);
void graphics_draw_rect(struct graphics_info *graphics_info, uint32_t x, uint32_t y, size_t width, size_t height, struct framebuffer_pixel pixel_color);
