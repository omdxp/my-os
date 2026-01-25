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
