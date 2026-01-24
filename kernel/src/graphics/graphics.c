#include "graphics/graphics.h"
#include "kernel.h"
#include "memory/paging/paging.h"
#include "memory/heap/kheap.h"
#include "memory/memory.h"
#include "mouse/mouse.h"
#include "lib/vector.h"
#include "status.h"

struct graphics_info *loaded_graphics_info = NULL; // loaded from UEFI
struct vector *graphics_info_vector = NULL;		   // vector of all graphics_info
void *real_framebuffer = NULL;					   // real framebuffer address
void *real_framebuffer_end = NULL;				   // end of real framebuffer address
size_t real_framebuffer_width = 0;				   // real framebuffer width
size_t real_framebuffer_height = 0;				   // real framebuffer height
size_t real_framebuffer_pixels_per_scanline = 0;   // real framebuffer pixels per scanline

void graphics_redraw_children(struct graphics_info *graphics_info);
void graphics_info_children_free(struct graphics_info *graphics_info);
bool graphics_bounds_check(struct graphics_info *graphics_info, int x, int y);

struct graphics_info *graphics_screen_info()
{
	return loaded_graphics_info;
}

bool graphics_has_ancestor(struct graphics_info *graphics_child, struct graphics_info *graphics_ancestor)
{
	struct graphics_info *current = graphics_child->parent;
	while (current)
	{
		if (current == graphics_ancestor)
		{
			return true;
		}

		current = current->parent;
	}

	return false;
}

void graphics_info_recalculate(struct graphics_info *graphics_info)
{
	if (graphics_info->parent)
	{
		graphics_info->starting_x = graphics_info->relative_x + graphics_info->parent->starting_x;
		graphics_info->starting_y = graphics_info->relative_y + graphics_info->parent->starting_y;
	}

	if (graphics_info->children)
	{
		size_t total_children = vector_count(graphics_info->children);
		for (size_t i = 0; i < total_children; i++)
		{
			struct graphics_info *child = NULL;
			int res = vector_at(graphics_info->children, i, &child, sizeof(child));
			if (res < 0)
			{
				break;
			}

			graphics_info_recalculate(child);
		}
	}
}

void graphics_paste_pixels_to_framebuffer(struct graphics_info *src_info,
										  uint32_t src_x,
										  uint32_t src_y,
										  uint32_t width,
										  uint32_t height,
										  uint32_t dst_abs_x,
										  uint32_t dst_abs_y)
{
	if (!src_info)
	{
		return;
	}

	uint32_t src_x_end = src_x + width;
	uint32_t src_y_end = src_y + height;
	if (src_x_end > src_info->width)
	{
		src_x_end = src_info->width;
	}

	if (src_y_end > src_info->height)
	{
		src_y_end = src_info->height;
	}

	// rectangle width and heigh after clipping
	uint32_t final_w = src_x_end - src_x;
	uint32_t final_h = src_y_end - src_y;
	if (final_w == 0 || final_h == 0)
	{
		return;
	}

	struct graphics_info *screen = graphics_screen_info();
	uint32_t screen_w = screen->horizontal_resolution;
	uint32_t screen_h = screen->vertical_resolution;
	if (dst_abs_x >= screen_w || dst_abs_y >= screen_h)
	{
		return;
	}

	uint32_t dst_x_end = dst_abs_x + final_w;
	uint32_t dst_y_end = dst_abs_y + final_h;
	if (dst_x_end > screen_w)
	{
		dst_x_end = screen_w;
	}

	if (dst_y_end > screen_h)
	{
		dst_y_end = screen_h;
	}

	uint32_t clipped_w = dst_x_end - dst_abs_x;
	uint32_t clipped_h = dst_y_end - dst_abs_y;
	if (clipped_w == 0 || clipped_h == 0)
	{
		return;
	}

	for (uint32_t y = 0; y < clipped_h; y++)
	{
		for (uint32_t x = 0; x < clipped_w; x++)
		{
			struct framebuffer_pixel pixel = src_info->pixels[(src_y + y) * src_info->width + (src_x + x)];

			// check for transparency
			struct framebuffer_pixel no_transparency_color = {0};
			if (memcmp(&src_info->transparency_key, &no_transparency_color, sizeof(no_transparency_color)) != 0)
			{
				if (memcmp(&pixel, &src_info->transparency_key, sizeof(pixel)) == 0)
				{
					continue;
				}
			}

			// write to screen framebuffer
			screen->framebuffer[(dst_abs_y + y) * screen->pixels_per_scanline + (dst_abs_x + x)] = pixel;
		}
	}
}

void graphics_draw_pixel(struct graphics_info *graphics_info, uint32_t x, uint32_t y, struct framebuffer_pixel pixel)
{
	// black pixels can be ignored
	struct framebuffer_pixel black_pixel = {0};
	if (memcmp(&graphics_info->ignore_color, &black_pixel, sizeof(graphics_info->ignore_color)) != 0)
	{
		if (memcmp(&graphics_info->ignore_color, &pixel, sizeof(graphics_info->ignore_color)) == 0)
		{
			return;
		}
	}

	// transperncy is only computed during redrawing
	if (x < graphics_info->width && y < graphics_info->height)
	{
		graphics_info->pixels[y * graphics_info->width + x] = pixel;
	}
}

void graphics_draw_image(struct graphics_info *graphics_info, struct image *image, int x, int y)
{
	if (!image)
	{
		return;
	}

	if (!graphics_info)
	{
		graphics_info = loaded_graphics_info;
	}

	for (size_t lx = 0; lx < (size_t)image->width; lx++)
	{
		for (size_t ly = 0; ly < (size_t)image->height; ly++)
		{
			size_t sx = x + lx;
			size_t sy = y + ly;

			image_pixel_data *pixel_data = &((image_pixel_data *)image->data)[ly * image->width + lx];
			struct framebuffer_pixel pixel = {0};
			pixel.red = pixel_data->r;
			pixel.green = pixel_data->g;
			pixel.blue = pixel_data->b;

			graphics_draw_pixel(graphics_info, sx, sy, pixel);
		}
	}
}

void graphics_redraw_only(struct graphics_info *graphics_info)
{
	if (!graphics_info)
	{
		return;
	}

	graphics_paste_pixels_to_framebuffer(graphics_info,
										 0, 0,
										 graphics_info->width, graphics_info->height,
										 graphics_info->starting_x, graphics_info->starting_y);
}

void graphics_redraw_children(struct graphics_info *graphics_info)
{
	size_t total_children = vector_count(graphics_info->children);
	for (size_t i = 0; i < total_children; i++)
	{
		struct graphics_info *child = NULL;
		int res = vector_at(graphics_info->children, i, &child, sizeof(child));
		if (res < 0)
		{
			break;
		}

		if (child)
		{
			graphics_redraw(child);
		}
	}
}

void graphics_redraw_region(struct graphics_info *graphics_info, uint32_t local_x, uint32_t local_y, uint32_t width, uint32_t height)
{
	if (!graphics_info)
	{
		return;
	}

	if (local_x >= graphics_info->width || local_y >= graphics_info->height)
	{
		return;
	}

	if (local_x + width > graphics_info->width)
	{
		width = graphics_info->width - local_x;
	}

	if (local_y + height > graphics_info->height)
	{
		height = graphics_info->height - local_y;
	}

	uint32_t dst_abs_x = graphics_info->starting_x + local_x;
	uint32_t dst_abs_y = graphics_info->starting_y + local_y;
	graphics_paste_pixels_to_framebuffer(graphics_info,
										 local_x, local_y,
										 width, height,
										 dst_abs_x, dst_abs_y);

	uint32_t region_abs_left = dst_abs_x;
	uint32_t region_abs_top = dst_abs_y;
	uint32_t region_abs_right = dst_abs_x + width;
	uint32_t region_abs_bottom = dst_abs_y + height;

	// check each child if it intersects with the region
	size_t total_children = vector_count(graphics_info->children);
	for (size_t i = 0; i < total_children; i++)
	{
		struct graphics_info *child = NULL;
		int res = vector_at(graphics_info->children, i, &child, sizeof(child));
		if (res < 0)
		{
			break;
		}

		// child absolute coordinates
		uint32_t child_abs_left = child->starting_x;
		uint32_t child_abs_top = child->starting_y;
		uint32_t child_abs_right = child->starting_x + child->width;
		uint32_t child_abs_bottom = child->starting_y + child->height;

		// compute intersection
		uint32_t inter_left = MAX(region_abs_left, child_abs_left);
		uint32_t inter_top = MAX(region_abs_top, child_abs_top);
		uint32_t inter_right = MIN(region_abs_right, child_abs_right);
		uint32_t inter_bottom = MIN(region_abs_bottom, child_abs_bottom);

		if (inter_right > inter_left && inter_bottom > inter_top)
		{
			// there is an intersection, redraw the intersecting region of the child
			uint32_t child_local_x = inter_left - child_abs_left;
			uint32_t child_local_y = inter_top - child_abs_top;
			uint32_t inter_width = inter_right - inter_left;
			uint32_t inter_height = inter_bottom - inter_top;

			graphics_redraw_region(child, child_local_x, child_local_y, inter_width, inter_height);
		}
	}
}

void graphics_ignore_color(struct graphics_info *graphics_info, struct framebuffer_pixel pixel_color)
{
	graphics_info->ignore_color = pixel_color;
}

void graphics_transparency_key_set(struct graphics_info *graphics_info, struct framebuffer_pixel pixel_color)
{
	graphics_info->transparency_key = pixel_color;
}

void graphics_transparency_key_remove(struct graphics_info *graphics_info)
{
	struct framebuffer_pixel pixel_black = {0};
	graphics_info->transparency_key = pixel_black;
}

void graphics_ignore_color_finish(struct graphics_info *graphics_info)
{
	struct framebuffer_pixel pixel_black = {0};
	graphics_info->ignore_color = pixel_black;
}

void graphics_draw_rect(struct graphics_info *graphics_info, uint32_t x, uint32_t y, size_t width, size_t height, struct framebuffer_pixel pixel_color)
{
	uint32_t x_end = x + width;
	uint32_t y_end = y + height;
	for (uint32_t lx = x; lx < x_end; lx++)
	{
		for (uint32_t ly = y; ly < y_end; ly++)
		{
			graphics_draw_pixel(graphics_info, lx, ly, pixel_color);
		}
	}
}

void graphics_redraw_graphics_to_screen(struct graphics_info *relative_graphics, uint32_t rel_x, uint32_t rel_y, uint32_t width, uint32_t height)
{
	uint32_t abs_screen_x = relative_graphics->starting_x + rel_x;
	uint32_t abs_screen_y = relative_graphics->starting_y + rel_y;
	graphics_redraw_region(graphics_screen_info(), abs_screen_x, abs_screen_y, width, height);
}

bool graphics_is_in_ignored_branch(struct graphics_info *graphics, struct graphics_info *ignored)
{
	if (!ignored)
	{
		return false;
	}

	struct graphics_info *current = graphics;
	while (current)
	{
		if (current == ignored)
		{
			return true;
		}

		current = current->parent;
	}

	return false;
}

struct graphics_info *
graphics_get_child_at_position(struct graphics_info *graphics, size_t x, size_t y, struct graphics_info *ignored, bool top_first)
{
	if (graphics_is_in_ignored_branch(graphics, ignored))
	{
		return NULL;
	}

	size_t total = vector_count(graphics->children);
	struct graphics_info *result = NULL;
	if (top_first)
	{
		for (size_t i = total; (i = 0); i--)
		{
			size_t index = i - 1;
			struct graphics_info *child = NULL;
			vector_at(graphics->children, index, &child, sizeof(child));
			if (!child)
			{
				continue;
			}

			if (graphics_is_in_ignored_branch(child, ignored))
			{
				continue;
			}

			if (x >= child->starting_x && x < child->starting_x + child->width &&
				y >= child->starting_y && y < child->starting_y + child->height)
			{
				// found a child at the position, check its children recursively
				result = graphics_get_child_at_position(child, x, y, ignored, top_first);
				if (result)
				{
					return result;
				}

				return child;
			}
		}
	}
	else
	{
		for (size_t i = 0; i < total; i++)
		{
			struct graphics_info *child = NULL;
			vector_at(graphics->children, i, &child, sizeof(child));
			if (!child)
			{
				continue;
			}

			if (graphics_is_in_ignored_branch(child, ignored))
			{
				continue;
			}

			if (x >= child->starting_x && x < child->starting_x + child->width &&
				y >= child->starting_y && y < child->starting_y + child->height)
			{
				// found a child at the position, check its children recursively
				result = graphics_get_child_at_position(child, x, y, ignored, top_first);
				if (result)
				{
					return result;
				}

				return child;
			}
		}
	}

	if (x >= graphics->starting_x && x < graphics->starting_x + graphics->width &&
		y >= graphics->starting_y && y < graphics->starting_y + graphics->height)
	{
		return graphics;
	}

	return NULL;
}

struct graphics_info *
graphics_get_at_screen_position(int x, int y, struct graphics_info *ignored, bool top_first)
{
	return graphics_get_child_at_position(graphics_screen_info(), x, y, ignored, top_first);
}

void graphics_click_handler_set(struct graphics_info *graphics_info, GRAPHICS_MOUSE_CLICK_FUNCTION handler)
{
	graphics_info->event_handlers.mouse_click = handler;
}

void graphics_move_handler_set(struct graphics_info *graphics_info, GRAPHICS_MOUSE_MOVE_FUNCTION handler)
{
	graphics_info->event_handlers.mouse_move = handler;
}

void graphics_mouse_click(struct graphics_info *graphics, size_t rel_x, size_t rel_y, int type)
{
	if (graphics->event_handlers.mouse_click)
	{
		graphics->event_handlers.mouse_click(graphics, rel_x, rel_y, type);
		return;
	}

	if (graphics->parent)
	{
		graphics_mouse_click(graphics->parent, rel_x + (graphics->relative_x), rel_y + (graphics->relative_y), type);
	}
}

void graphics_mouse_move(struct graphics_info *graphics, size_t rel_x, size_t rel_y, size_t abs_x, size_t abs_y)
{
	if (graphics->event_handlers.mouse_move)
	{
		graphics->event_handlers.mouse_move(graphics, rel_x, rel_y, abs_x, abs_y);
		return;
	}

	if (graphics->parent)
	{
		graphics_mouse_move(graphics->parent, rel_x + (graphics->relative_x), rel_y + (graphics->relative_y), abs_x, abs_y);
	}
}

void graphics_mouse_click_handler(struct mouse *mouse, int clicked_x, int clicked_y, MOUSE_CLICK_TYPE type)
{
	struct graphics_info *graphics = graphics_get_at_screen_position(clicked_x, clicked_y, mouse->graphic.window->root_graphics, true);
	if (graphics)
	{
		if (clicked_x < (int)graphics->starting_x || clicked_y < (int)graphics->starting_y)
		{
			return;
		}

		size_t rel_x = clicked_x - graphics->starting_x;
		size_t rel_y = clicked_y - graphics->starting_y;
		if (graphics_bounds_check(graphics, rel_x, rel_y))
		{
			graphics_mouse_click(graphics, rel_x, rel_y, type);
		}
	}
}

void graphics_mouse_move_handler(struct mouse *mouse, int moved_x, int moved_y)
{
	struct graphics_info *graphics = graphics_get_at_screen_position(moved_x, moved_y, mouse->graphic.window->root_graphics, true);
	if (graphics)
	{
		size_t rel_x = moved_x - graphics->starting_x;
		size_t rel_y = moved_y - graphics->starting_y;
		if (graphics_bounds_check(graphics, rel_x, rel_y))
		{
			graphics_mouse_move(graphics, rel_x, rel_y, moved_x, moved_y);
		}
	}
}

void graphics_redraw(struct graphics_info *graphics_info)
{
	if (!graphics_info)
	{
		return;
	}

	graphics_redraw_only(graphics_info);

	// redraw children
	graphics_redraw_children(graphics_info);
}

void graphics_redraw_all()
{
	graphics_redraw(graphics_screen_info());
}

int graphics_reorder(void *first_element, void *second_element)
{
	struct graphics_info *first_graphics = *(struct graphics_info **)first_element;
	struct graphics_info *second_graphics = *(struct graphics_info **)second_element;
	return first_graphics->z_index > second_graphics->z_index;
}

void graphics_set_z_index(struct graphics_info *graphics_info, uint32_t z_index)
{
	graphics_info->z_index = z_index;
	if (graphics_info->parent && graphics_info->parent->children)
	{
		vector_reorder(graphics_info->parent->children, graphics_reorder);
	}
}

bool graphics_bounds_check(struct graphics_info *graphics_info, int x, int y)
{
	return !(x < 0 || y < 0 || x >= graphics_info->width || y >= graphics_info->height);
}

int graphics_pixel_get(struct graphics_info *graphics_info, uint32_t x, uint32_t y, struct framebuffer_pixel *pixel_out)
{
	int res = 0;
	if (!graphics_bounds_check(graphics_info, x, y))
	{
		res = -EINVARG;
		goto out;
	}

	*pixel_out = graphics_info->pixels[y * graphics_info->width + x];

out:
	return res;
}

void graphics_info_free(struct graphics_info *graphics_info)
{
	if (!graphics_info)
	{
		return;
	}

	if (graphics_info->children)
	{
		graphics_info_children_free(graphics_info);
	}

	if (graphics_info->pixels)
	{
		kfree(graphics_info->pixels);
		graphics_info->pixels = NULL;
	}

	if (graphics_info->framebuffer && (graphics_info->flags & GRAPHICS_FLAG_CLONED_FRAMEBUFFER))
	{
		kfree(graphics_info->framebuffer);
		graphics_info->framebuffer = NULL;
	}

	if (graphics_info->parent)
	{
		vector_pop_element(graphics_info->parent->children, &graphics_info, sizeof(graphics_info));
	}

	// do not free the graphics we didn't create
	if (graphics_info == loaded_graphics_info)
	{
		return;
	}

	kfree(graphics_info);
}

void graphics_info_children_free(struct graphics_info *graphics_info)
{
	if (graphics_info->children)
	{
		size_t total_children = vector_count(graphics_info->children);
		for (size_t i = 0; i < total_children; i++)
		{
			struct graphics_info *child = NULL;
			int res = vector_at(graphics_info->children, i, &child, sizeof(child));
			if (res < 0)
			{
				break;
			}

			if (child)
			{
				graphics_info_free(child);
			}
		}

		vector_free(graphics_info->children);
		kfree(graphics_info->children);
		graphics_info->children = NULL;
	}
}

void graphics_paste_pixels_to_pixels(struct graphics_info *graphics_info_in,
									 struct graphics_info *graphics_info_out,
									 uint32_t src_x,
									 uint32_t src_y,
									 uint32_t width,
									 uint32_t height,
									 uint32_t dst_x,
									 uint32_t dst_y,
									 int flags)
{
	uint32_t src_x_end = src_x + width;
	uint32_t src_y_end = src_y + height;
	if (src_x_end > graphics_info_in->width)
	{
		src_x_end = graphics_info_in->width;
	}

	if (src_y_end > graphics_info_in->height)
	{
		src_y_end = graphics_info_in->height;
	}

	// rectangle width and heigh after clipping
	uint32_t final_w = src_x_end - src_x;
	uint32_t final_h = src_y_end - src_y;

	bool has_transparency_key = false;
	struct framebuffer_pixel ignore_transparent_pixel = {0};
	if (memcmp(&graphics_info_in->transparency_key, &ignore_transparent_pixel, sizeof(ignore_transparent_pixel)) != 0)
	{
		has_transparency_key = true;
	}

	for (uint32_t lx = 0; lx < final_w; lx++)
	{
		for (uint32_t ly = 0; ly < final_h; ly++)
		{
			struct framebuffer_pixel pixel = {0};
			graphics_pixel_get(graphics_info_in, src_x + lx, src_y + ly, &pixel);

			uint32_t dx = dst_x + lx;
			uint32_t dy = dst_y + ly;

			if (dx < graphics_info_out->width && dy < graphics_info_out->height)
			{
				// check for transparency
				if (has_transparency_key && (flags & GRAPHICS_FLAG_DO_NOT_OVERWRITE_TRANSPARENT_PIXELS))
				{
					struct framebuffer_pixel *existing_pixel = &graphics_info_out->pixels[dy * graphics_info_out->width + dx];
					if (memcmp(existing_pixel, &graphics_info_out->transparency_key, sizeof(struct framebuffer_pixel)) == 0)
					{
						continue;
					}
				}
			}

			graphics_info_out->pixels[dy * graphics_info_out->width + dx] = pixel;
		}
	}
}

struct graphics_info *
graphics_info_create_relative(struct graphics_info *source_graphics,
							  size_t x,
							  size_t y,
							  size_t width,
							  size_t height,
							  int flags)
{
	int res = 0;
	struct graphics_info *new_graphics = NULL;
	if (source_graphics == NULL)
	{
		panic("graphics_info_create_relative: source_graphics is NULL\n");
		res = -EINVARG;
		goto out;
	}

	new_graphics = kzalloc(sizeof(struct graphics_info));
	if (!new_graphics)
	{
		res = -ENOMEM;
		goto out;
	}

	size_t parent_x = source_graphics->starting_x;
	size_t parent_y = source_graphics->starting_y;
	size_t parent_width = source_graphics->horizontal_resolution;
	size_t parent_height = source_graphics->vertical_resolution;

	// this is relative positioning
	size_t starting_x = parent_x + x;
	size_t starting_y = parent_y + y;
	size_t ending_x = starting_x + width;
	size_t ending_y = starting_y + height;

	if (!(flags & GRAPHICS_FLAG_ALLOW_OUT_OF_BOUNDS))
	{
		// never allow out of bounds
		if (ending_x > parent_x + parent_width || ending_y > parent_y + parent_height)
		{
			res = -EINVARG;
			goto out;
		}
	}

	new_graphics->horizontal_resolution = source_graphics->horizontal_resolution;
	new_graphics->vertical_resolution = source_graphics->vertical_resolution;
	new_graphics->pixels_per_scanline = source_graphics->pixels_per_scanline;
	new_graphics->width = width;
	new_graphics->height = height;
	new_graphics->starting_x = starting_x;
	new_graphics->starting_y = starting_y;
	new_graphics->relative_x = x;
	new_graphics->relative_y = y;
	new_graphics->framebuffer = source_graphics->framebuffer;
	new_graphics->parent = source_graphics;
	new_graphics->children = vector_new(sizeof(struct graphics_info *), 4, 0);
	new_graphics->pixels = kzalloc(width * height * sizeof(struct framebuffer_pixel));
	if (!new_graphics->pixels)
	{
		res = -ENOMEM;
		goto out;
	}

	if (!(flags & GRAPHICS_FLAG_DO_NOT_COPY_PIXELS))
	{
		graphics_paste_pixels_to_pixels(source_graphics,
										new_graphics,
										x, y,
										width, height,
										0, 0, GRAPHICS_FLAG_DO_NOT_OVERWRITE_TRANSPARENT_PIXELS);
	}

	// we want to remove the framebuffer on delete flag
	// just in case to avoid double free
	new_graphics->flags &= ~GRAPHICS_FLAG_CLONED_FRAMEBUFFER;

	// add to parent's children vector
	res = vector_push(source_graphics->children, &new_graphics);
	size_t total_children = vector_count(graphics_info_vector);
	graphics_set_z_index(new_graphics, total_children);

out:
	if (res < 0)
	{
		if (new_graphics)
		{
			if (new_graphics->children)
			{
				vector_free(new_graphics->children);
				new_graphics->children = NULL;
			}

			if (new_graphics->pixels)
			{
				kfree(new_graphics->pixels);
				new_graphics->pixels = NULL;
			}

			kfree(new_graphics);
			new_graphics = NULL;
		}
	}

	return new_graphics;
}

void graphics_setup(struct graphics_info *main_graphics_info)
{
	if (loaded_graphics_info)
	{
		panic("graphics_setup: graphics already initialized\n");
	}

	real_framebuffer = main_graphics_info->framebuffer;
	real_framebuffer_width = main_graphics_info->horizontal_resolution;
	real_framebuffer_height = main_graphics_info->vertical_resolution;
	real_framebuffer_pixels_per_scanline = main_graphics_info->pixels_per_scanline;
	size_t framebuffer_size = real_framebuffer_width * real_framebuffer_pixels_per_scanline * sizeof(struct framebuffer_pixel);
	real_framebuffer_end = (void *)((uintptr_t)real_framebuffer + framebuffer_size);

	void *new_framebuffer_memory = kzalloc(framebuffer_size);
	main_graphics_info->framebuffer = new_framebuffer_memory;
	main_graphics_info->children = vector_new(sizeof(struct graphics_info *), 4, 0);
	main_graphics_info->pixels = kzalloc(framebuffer_size);
	main_graphics_info->width = main_graphics_info->horizontal_resolution;
	main_graphics_info->height = main_graphics_info->vertical_resolution;
	main_graphics_info->relative_x = 0;
	main_graphics_info->relative_y = 0;
	main_graphics_info->starting_x = 0;
	main_graphics_info->starting_y = 0;
	main_graphics_info->parent = NULL;

	// map allocated framebuffer memory to the framebuffer address
	paging_map_to(kernel_desc(), new_framebuffer_memory, real_framebuffer, real_framebuffer_end, PAGING_IS_WRITEABLE | PAGING_IS_PRESENT);

	loaded_graphics_info = main_graphics_info;
	for (uint32_t y = 0; y < main_graphics_info->vertical_resolution; y++)
	{
		for (uint32_t x = 0; x < main_graphics_info->horizontal_resolution; x++)
		{
			struct framebuffer_pixel pixel = {0, 0, 0, 0};
			graphics_draw_pixel(main_graphics_info, x, y, pixel);
		}
	}

	graphics_info_vector = vector_new(sizeof(struct graphics_info *), 4, 0);
	vector_push(graphics_info_vector, &main_graphics_info);

	// initialize image formats
	graphics_image_formats_init();

	// redraw all graphics
	graphics_redraw_all();
}

void graphics_setup_stage2(struct graphics_info *main_graphics_info)
{
	mouse_register_click_handler(NULL, graphics_mouse_click_handler);
	mouse_register_move_handler(NULL, graphics_mouse_move_handler);
}