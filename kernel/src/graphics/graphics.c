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
			if (memcmp(&src_info->transparency_key, &no_transparency_color, sizeof(struct framebuffer_pixel)) != 0)
			{
				if (memcmp(&pixel, &src_info->transparency_key, sizeof(struct framebuffer_pixel)) == 0)
				{
					continue;
				}
			}

			// write to screen framebuffer
			screen->framebuffer[(dst_abs_y + y) * screen->pixels_per_scanline + (dst_abs_x + x)] = pixel;
		}
	}
}

void graphics_draw_pixel(struct graphics_info *graphics_info, size_t x, size_t y, struct framebuffer_pixel pixel)
{
	// black pixels can be ignored
	struct framebuffer_pixel black_pixel = {0};
	if (memcmp(&graphics_info->ignore_color, &black_pixel, sizeof(struct framebuffer_pixel)) != 0)
	{
		if (memcmp(&pixel, &graphics_info->ignore_color, sizeof(struct framebuffer_pixel)) == 0)
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
		graphics_info = graphics_screen_info();
	}

	for (size_t lx = 0; lx < (size_t)image->width; lx++)
	{
		for (size_t ly = 0; ly < (size_t)image->height; ly++)
		{
			size_t sx = x + lx;
			size_t sy = y + ly;

			image_pixel_data *pixel_data = &((image_pixel_data *)image->data)[ly * image->width + lx];
			struct framebuffer_pixel pixel = {pixel_data->b, pixel_data->g, pixel_data->r, pixel_data->a};
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

void graphics_redraw(struct graphics_info *graphics_info)
{
	if (!graphics_info)
	{
		return;
	}

	graphics_redraw_only(graphics_info);

	// TODO: redraw children
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
