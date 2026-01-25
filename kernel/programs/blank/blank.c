#include "myos.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "file.h"

struct window
{
	char title[64];
	int width;
	int height;
};

struct framebuffer_pixel
{
	uint8_t blue;
	uint8_t green;
	uint8_t red;
	uint8_t reserved;
};

struct userland_graphics
{
	size_t x;
	size_t y;
	size_t width;
	size_t height;

	void *pixels;		// pixels array
	void *userland_ptr; // pointer to graphics in userland process
};

int main(int argc, char **argv)
{
	struct window *win = myos_window_create("Blank Program", 800, 600, 0, 0);
	if (win)
	{
		printf("Created window titled: %s\n", win->title);
	}
	else
	{
		printf("Failed to create window\n");
	}

	// divert stdout to the created window
	myos_divert_stdout_to_window(win);

	struct userland_graphics *graphics = myos_window_get_graphics(win);
	if (!graphics)
	{
		printf("Failed to get graphics context for window\n");
		return -1;
	}

	struct framebuffer_pixel *pixels = myos_graphics_get_pixels(graphics);
	struct framebuffer_pixel blue = {255, 0, 0, 0};
	for (int y = 0; y < graphics->height; y++)
	{
		for (int x = 0; x < graphics->width; x++)
		{
			pixels[y * graphics->width + x] = blue;
		}
	}

	myos_window_redraw(win);

	while (1)
	{
		struct window_event window_event = {0};
		int res = myos_process_get_window_event(&window_event);
		if (res >= 0)
		{
			printf("Received window event of type: %d from window ID: %d\n", window_event.type, window_event.win_id);
			switch (window_event.type)
			{
			case 4: // click event
				printf("Mouse click at position (%d, %d)\n", window_event.data.click.x, window_event.data.click.y);
				break;

			default:
				break;
			}
		}
	}

	return 0;
}
