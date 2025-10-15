#pragma once

#include "font.h"
#include "graphics.h"
#include <stdint.h>
#include <stddef.h>

enum
{
	TERMINAL_FLAG_BACKSPACE_ALLOWED = 0b00000001,
};

struct terminal
{
	struct graphics_info *graphics_info;		   // pointer to the graphics_info for rendering
	struct framebuffer_pixel *terminal_background; // background color of the terminal

	struct
	{
		size_t row;
		size_t col;
	} text; // position of the next character to be printed

	struct
	{
		size_t abs_x;
		size_t abs_y;
		size_t width;
		size_t height;
	} bounds; // absolute bounds of the terminal

	struct font *font;					 // pointer to the font used for rendering text
	struct framebuffer_pixel font_color; // color of the font

	int flags; // flags for terminal behavior
};

void terminal_system_setup();
struct terminal *terminal_create(struct graphics_info *graphics_info, int starting_x, int starting_y, size_t width, size_t height, struct font *font, struct framebuffer_pixel font_color, int flags);
void terminal_free(struct terminal *term);
void terminal_background_save(struct terminal *term);
struct terminal *terminal_get_at_screen_position(size_t x, size_t y, struct terminal *ignore_terminal);
int terminal_print(struct terminal *term, const char *str);
int terminal_draw_rect(struct terminal *term, uint32_t x, uint32_t y, size_t width, size_t height, struct framebuffer_pixel pixel_color);
int terminal_draw_image(struct terminal *term, uint32_t x, uint32_t y, struct image *image);
void terminal_ignore_color_finish(struct terminal *term);
void terminal_ignore_color(struct terminal *term, struct framebuffer_pixel pixel_color);
void terminal_transparency_key_remove(struct terminal *term);
void terminal_transparency_key_set(struct terminal *term, struct framebuffer_pixel pixel_color);
int terminal_pixel_set(struct terminal *term, size_t x, size_t y, struct framebuffer_pixel pixel_color);
int terminal_write(struct terminal *term, int c);
int terminal_backspace(struct terminal *term);
void terminal_restore_background(struct terminal *term, size_t sx, size_t sy, size_t width, size_t height);
int terminal_total_rows(struct terminal *term);
int terminal_total_columns(struct terminal *term);
int terminal_cursor_column(struct terminal *term);
int terminal_cursor_row(struct terminal *term);
int terminal_cursor_set(struct terminal *term, size_t row, size_t col);
