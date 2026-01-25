#pragma once

#include <stdint.h>
#include <stddef.h>

struct graphics_info;
struct process;
struct interrupt_frame;

struct userland_graphics
{
	size_t x;
	size_t y;
	size_t width;
	size_t height;

	void *pixels;		// pixels array
	void *userland_ptr; // pointer to graphics in userland process
};

struct userland_graphics *isr80h_graphics_make_userland_metadata(struct process *process, struct graphics_info *graphics_info);
void *isr80h_command20_graphics_pixels_buffer_get(struct interrupt_frame *frame);