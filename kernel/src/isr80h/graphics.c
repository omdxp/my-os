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