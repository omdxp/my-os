#include "graphics.h"
#include "graphics/graphics.h"
#include "task/task.h"
#include "task/process.h"
#include "task/userlandptr.h"

struct userland_graphics *isr80h_graphics_make_userland_metadata(struct process *process, struct graphics_info *graphics_info)
{
	struct userland_ptr *userland_graphics_ptr = process_userland_pointer_create(process, graphics_info);
	if (!userland_graphics_ptr)
	{
		return NULL;
	}

	struct userland_graphics *userland_graphics_metadata = process_malloc(process, sizeof(struct userland_graphics));
	if (!userland_graphics_metadata)
	{
		return NULL;
	}

	userland_graphics_metadata->width = graphics_info->width;
	userland_graphics_metadata->height = graphics_info->height;
	userland_graphics_metadata->x = graphics_info->relative_x;
	userland_graphics_metadata->y = graphics_info->relative_y;
	userland_graphics_metadata->userland_ptr = userland_graphics_ptr;
	return userland_graphics_metadata;
}

void *isr80h_command20_graphics_pixels_buffer_get(struct interrupt_frame *frame)
{
	int res = 0;
	struct framebuffer_pixel *graphics_pixels_virt_out = NULL;
	struct userland_graphics *userland_graphics_ptr = NULL;
	userland_graphics_ptr = (struct userland_graphics *)task_get_stack_item(task_current(), 0);
	if (!userland_graphics_ptr)
	{
		return NULL;
	}

	userland_graphics_ptr = task_virtual_addr_to_phys(task_current(), userland_graphics_ptr);
	if (!userland_graphics_ptr)
	{
		return NULL;
	}

	struct graphics_info *kernel_land_graphics_ptr = process_userland_pointer_kernel_ptr(task_current()->process, (struct userland_ptr *)userland_graphics_ptr->userland_ptr);
	if (!kernel_land_graphics_ptr)
	{
		return NULL;
	}

	struct framebuffer_pixel *pixels = kernel_land_graphics_ptr->pixels;
	if (!pixels)
	{
		return NULL;
	}

	res = process_map_graphics_framebuffer_pixels_into_userspace(task_current()->process, kernel_land_graphics_ptr, &graphics_pixels_virt_out, NULL);
	if (res < 0)
	{
		return NULL;
	}

	return graphics_pixels_virt_out;
}

void *isr80h_command22_graphics_create(struct interrupt_frame *frame)
{
	size_t x = 0;
	size_t y = 0;
	size_t width = 0;
	size_t height = 0;

	x = (size_t)task_get_stack_item(task_current(), 4);
	y = (size_t)task_get_stack_item(task_current(), 3);
	width = (size_t)task_get_stack_item(task_current(), 2);
	height = (size_t)task_get_stack_item(task_current(), 1);

	struct userland_graphics *userland_graphics_ptr = task_get_stack_item(task_current(), 0);
	if (!userland_graphics_ptr)
	{
		return NULL;
	}

	userland_graphics_ptr = (struct userland_graphics *)task_virtual_addr_to_phys(task_current(), userland_graphics_ptr);
	struct graphics_info *kernel_land_graphics_ptr = process_userland_pointer_kernel_ptr(task_current()->process, (struct userland_ptr *)userland_graphics_ptr->userland_ptr);
	if (!kernel_land_graphics_ptr)
	{
		return NULL;
	}

	struct graphics_info *child_graphics = graphics_info_create_relative(kernel_land_graphics_ptr, x, y, width, height, 0);
	if (!child_graphics)
	{
		return NULL;
	}

	struct userland_ptr *userland_child_graphics = process_userland_pointer_create(task_current()->process, child_graphics);
	if (!userland_child_graphics)
	{
		return NULL;
	}

	struct userland_graphics *userland_child_graphics_metadata = process_malloc(task_current()->process, sizeof(struct userland_graphics));
	if (!userland_child_graphics_metadata)
	{
		return NULL;
	}

	userland_child_graphics_metadata->width = child_graphics->width;
	userland_child_graphics_metadata->height = child_graphics->height;
	userland_child_graphics_metadata->x = child_graphics->relative_x;
	userland_child_graphics_metadata->y = child_graphics->relative_y;
	userland_child_graphics_metadata->userland_ptr = userland_child_graphics;

	return (void *)userland_child_graphics_metadata;
}