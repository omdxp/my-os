#include "graphics.h"
#include "kernel.h"
#include "memory/paging/paging.h"
#include "memory/heap/kheap.h"
#include "memory/memory.h"
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

struct graphics_info *graphics_screen_info()
{
	return loaded_graphics_info;
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
			continue;
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
		return;
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
			continue;
		}

		// child absolute coordinates
		uint32_t child_abs_left = child->starting_x;
		uint32_t child_abs_top = child->starting_y;
		uint32_t child_abs_right = child->starting_x + child->width;
		uint32_t child_abs_bottom = child->starting_y + child->height;

		// compute intersection
		uint32_t inter_left = MAX(region_abs_left, child_abs_left);
		uint32_t inter_top = MAX(region_abs_top, child_abs_top);
		uint32_t inter_right = MAX(region_abs_right, child_abs_right);
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
}
