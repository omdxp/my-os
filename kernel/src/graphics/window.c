#include "window.h"

struct vector *windows_vector; // vector to hold all windows

struct image *close_icon = NULL; // close icon image

struct window *moving_window = NULL; // currently moving window

struct window *focused_window = NULL; // currently focused window

int window_autoincrement_id = 100000; // auto-incrementing window ID counter

int window_system_initialize()
{
	int res = 0;
	windows_vector = vector_new(sizeof(struct window *), 10, 0);
	if (!windows_vector)
	{
		res = -ENOMEM;
		goto out;
	}

	close_icon = graphics_image_load("@:/closeico.bmp");
	if (!close_icon)
	{
		res = -EIO;
		goto out;
	}

	moving_window = NULL;
	focused_window = NULL;
	window_autoincrement_id = 100000;
out:
	return res;
}

int window_system_initialize_stage2()
{
	// TODO: register mouse move and click handlers and keyboard listeners
	return 0;
}

void window_draw_title_bar(struct window *window, struct framebuffer_pixel title_bar_bg_color)
{
	if (!window || !window->title_bar_graphics)
	{
		return;
	}

	size_t total_window_width_bounds = window->title_bar_graphics->width;
	size_t icon_pos_x = window->title_bar_components.close_button.x;
	size_t icon_pos_y = window->title_bar_components.close_button.y;
	const char *title = window->title;

	// draw background of title bar
	terminal_draw_rect(window->title_bar_terminal, 0, 0, total_window_width_bounds, WINDOW_TITLE_BAR_HEIGHT, title_bar_bg_color);

	// draw title text
	terminal_cursor_set(window->title_bar_terminal, 0, 0);
	terminal_print(window->title_bar_terminal, title);

	// draw close icon ignoring white color
	struct framebuffer_pixel ignore_color = {0xff, 0xff, 0xff, 0x00};
	terminal_ignore_color(window->title_bar_terminal, ignore_color);
	terminal_draw_image(window->title_bar_terminal, icon_pos_x, icon_pos_y, close_icon);
	terminal_ignore_color_finish(window->title_bar_terminal);
}

int window_reorder(void *first_element, void *second_element)
{
	struct window *first_window = *(struct window **)first_element;
	struct window *second_window = *(struct window **)second_element;

	return first_window->z_index < second_window->z_index;
}

void window_set_z_index(struct window *window, int z_index)
{
	graphics_set_z_index(window->root_graphics, z_index);

	// reorder windows vector based on z-index
	vector_reorder(windows_vector, window_reorder);
}

void window_unfocus(struct window *window)
{
	struct framebuffer_pixel black = {0};
	window_draw_title_bar(window, black);
	graphics_redraw_region(graphics_screen_info(), window->root_graphics->starting_x, window->root_graphics->starting_y, window->root_graphics->width, window->root_graphics->height);

	// TODO: setup unfocus event handler
}

void window_bring_to_top(struct window *window)
{
	size_t last_index = 0;
	struct graphics_info *screen_graphics = graphics_screen_info();
	size_t child_count = vector_count(screen_graphics->children);
	if (child_count > 0)
	{
		struct graphics_info *child_graphics = NULL;
		size_t child_index = child_count - 1;
		vector_at(screen_graphics->children, child_index, &child_graphics, sizeof(child_graphics));
		if (child_graphics)
		{
			last_index = child_graphics->z_index;
		}
	}

	window_set_z_index(window, last_index + 1);
}

void window_focus(struct window *window)
{
	if (!window)
	{
		return;
	}

	if (focused_window == window)
	{
		return;
	}

	struct window *old_focused_window = focused_window;
	focused_window = window;
	struct framebuffer_pixel red = {0};
	red.red = 0xff;
	if (old_focused_window && old_focused_window->title_bar_graphics)
	{
		window_unfocus(old_focused_window);
	}

	// bring new focused window to top
	window_bring_to_top(window);

	// update new window title bar color
	if (window->title_bar_graphics)
	{
		window_draw_title_bar(window, red);
	}

	// force redraw of of the window
	graphics_redraw_graphics_to_screen(window->root_graphics, 0, 0, window->root_graphics->width, window->root_graphics->height);
}

void window_event_handler_unregister(struct window *window, WINDOW_EVENT_HANDLER handler)
{
	vector_pop_element(window->event_handlers.handlers, &handler, sizeof(WINDOW_EVENT_HANDLER));
}

void window_event_handler_register(struct window *window, WINDOW_EVENT_HANDLER handler)
{
	vector_push(window->event_handlers.handlers, &handler);
}

void window_drop_event_handlers(struct window *window)
{
	WINDOW_EVENT_HANDLER handler = NULL;
	vector_at(window->event_handlers.handlers, 0, &handler, sizeof(WINDOW_EVENT_HANDLER));
	while (handler)
	{
		window_event_handler_unregister(window, handler); // this will pop the handler from the vector
		vector_at(window->event_handlers.handlers, 0, &handler, sizeof(WINDOW_EVENT_HANDLER));
	}
}

void window_free(struct window *window)
{
	// drop event handlers
	window_drop_event_handlers(window);

	// free event handlers vector
	vector_free(window->event_handlers.handlers);

	vector_pop_element(windows_vector, &window, sizeof(struct window *));
	terminal_free(window->terminal);

	// free title bar terminal
	terminal_free(window->title_bar_terminal);

	// free root graphics info
	// this will also free all child graphics infos
	graphics_info_free(window->root_graphics);

	kfree(window);
}

void window_event_push(struct window *window, struct window_event *event)
{
	event->window = window;
	event->win_id = window->id;

	size_t total_handlers = vector_count(window->event_handlers.handlers);
	for (size_t i = 0; i < total_handlers; i++)
	{
		WINDOW_EVENT_HANDLER handler = NULL;
		vector_at(window->event_handlers.handlers, i, &handler, sizeof(WINDOW_EVENT_HANDLER));
		if (handler)
		{
			handler(window, event);
		}
	}
}

void window_close(struct window *window)
{
	struct window_event event = {0};
	event.type = WINDOW_EVENT_TYPE_WINDOW_CLOSE;
	window_event_push(window, &event);

	window_free(window);
	graphics_redraw_all();
}

int window_event_handler(struct window *window, struct window_event *event)
{
	// TODO: handle window events here
	return 0;
}

void window_redraw(struct window *window)
{
	graphics_redraw(window->root_graphics);
}

int window_position_set(struct window *window, size_t new_x, size_t new_y)
{
	int res = 0;

	int x_redraw_x = 0;
	int x_redraw_y = 0;
	int x_redraw_width = 0;
	int x_redraw_height = 0;

	int y_redraw_x = 0;
	int y_redraw_y = 0;
	int y_redraw_width = 0;
	int y_redraw_height = 0;

	struct graphics_info *screen = graphics_screen_info();
	size_t ending_x = new_x + window->width;
	size_t ending_y = new_y + window->height;
	if (ending_x > screen->width)
	{
		new_x = screen->width - window->width - 1;
	}

	if (ending_y > screen->height)
	{
		new_y = screen->height - window->height - 1;
	}

	int old_screen_x = window->root_graphics->starting_x;
	int old_screen_y = window->root_graphics->starting_y;

	window->root_graphics->relative_x = new_x;
	window->root_graphics->relative_y = new_y;
	window->root_graphics->starting_x = new_x;
	window->root_graphics->starting_y = new_y;

	window->x = new_x;
	window->y = new_y;

	window_bring_to_top(window);

	graphics_info_recalculate(window->root_graphics);

	int x_gap = old_screen_x - (int)window->root_graphics->starting_x;
	int y_gap = old_screen_y - (int)window->root_graphics->starting_y;

	bool moved_left = x_gap >= 0;
	bool moved_up = y_gap >= 0;

	x_redraw_x = window->root_graphics->starting_x + window->root_graphics->width;
	x_redraw_width = x_gap;
	x_redraw_y = old_screen_y;
	x_redraw_height = window->root_graphics->height;
	if (!moved_left)
	{
		x_redraw_x = window->root_graphics->starting_x + x_gap;
		x_redraw_width = -x_gap;
	}

	y_redraw_x = old_screen_x;
	y_redraw_y = window->root_graphics->starting_y + window->root_graphics->height;
	y_redraw_width = window->root_graphics->width;
	y_redraw_height = y_gap;
	if (!moved_up)
	{
		y_redraw_y = window->root_graphics->starting_y + y_gap;
		y_redraw_height = -y_gap;
	}

	if (x_redraw_width > window->root_graphics->width || x_redraw_height > window->root_graphics->height || y_redraw_width > window->root_graphics->width || y_redraw_height > window->root_graphics->height)
	{
		graphics_redraw_region(screen, old_screen_x, old_screen_y, window->root_graphics->width, window->root_graphics->height);
	}
	else
	{
		graphics_redraw_region(screen, x_redraw_x, x_redraw_y, x_redraw_width, x_redraw_height);
		graphics_redraw_region(screen, y_redraw_x, y_redraw_y, y_redraw_width, y_redraw_height);
	}

	window_redraw(window);
	return res;
}

struct window *
window_create(struct graphics_info *graphics_info, struct font *font, const char *title, size_t x, size_t y, size_t width, size_t height, int flags, int id)
{
	int res = 0;
	if (!windows_vector)
	{
		panic("window_create: window system not initialized");
	}

	if (width < 1 || height < 1)
	{
		res = -EINVARG;
		goto out;
	}

	if (!font)
	{
		font = font_get_system_font();
	}

	struct window *window = kzalloc(sizeof(struct window));
	if (!window)
	{
		res = -ENOMEM;
		goto out;
	}

	if (id == -1)
	{
		id = window_autoincrement_id++;
	}

	strncpy(window->title, title, sizeof(window->title));
	window->x = x;
	window->y = y;
	window->width = width;
	window->height = height;
	window->flags = flags;
	window->id = id;

	// setup event handlers vector
	window->event_handlers.handlers = vector_new(sizeof(WINDOW_EVENT_HANDLER), 4, 0);

	size_t total_window_width_bounds = width;
	size_t total_window_height_bounds = height;
	size_t window_body_height_offset = 0;
	size_t window_body_width_offset = 0;

	struct graphics_info *title_bar_graphics_info = NULL;
	struct graphics_info *border_left_graphics_info = NULL;
	struct graphics_info *border_right_graphics_info = NULL;
	struct graphics_info *border_bottom_graphics_info = NULL;

	if (!(flags & WINDOW_FLAG_BORDERLESS))
	{
		total_window_width_bounds += WINDOW_BORDER_PIXEL_SIZE * 2;
		total_window_height_bounds += WINDOW_TITLE_BAR_HEIGHT + WINDOW_BORDER_PIXEL_SIZE;
		window_body_height_offset = WINDOW_TITLE_BAR_HEIGHT;
		window_body_width_offset = WINDOW_BORDER_PIXEL_SIZE;
	}

	struct graphics_info *root_graphics_info = graphics_info_create_relative(graphics_info, x, y, total_window_width_bounds, total_window_height_bounds, GRAPHICS_FLAG_DO_NOT_COPY_PIXELS);
	if (!root_graphics_info)
	{
		res = -ENOMEM;
		goto out;
	}

	if (flags & WINDOW_FLAG_BACKGROUND_TRANSPARENT)
	{
		struct framebuffer_pixel transparency_key = {0};
		transparency_key.blue = 0xff;
		transparency_key.green = 0xff;
		transparency_key.red = 0xff;
		graphics_transparency_key_set(root_graphics_info, transparency_key);
		graphics_draw_rect(root_graphics_info, 0, 0, root_graphics_info->width, root_graphics_info->height, transparency_key);
	}

	window->root_graphics = root_graphics_info;
	if (!(flags & WINDOW_FLAG_BORDERLESS))
	{
		title_bar_graphics_info = graphics_info_create_relative(root_graphics_info, WINDOW_BORDER_PIXEL_SIZE, 0, width, WINDOW_TITLE_BAR_HEIGHT, 0);
		if (!title_bar_graphics_info)
		{
			res = -ENOMEM;
			goto out;
		}

		// TODO: setup click handler
		// TODO: setup move handler

		window->title_bar_graphics = title_bar_graphics_info;

		border_left_graphics_info = graphics_info_create_relative(root_graphics_info, 0, WINDOW_TITLE_BAR_HEIGHT, WINDOW_BORDER_PIXEL_SIZE, height, 0);
		if (!border_left_graphics_info)
		{
			res = -ENOMEM;
			goto out;
		}

		border_right_graphics_info = graphics_info_create_relative(root_graphics_info, total_window_width_bounds - WINDOW_BORDER_PIXEL_SIZE, WINDOW_TITLE_BAR_HEIGHT, WINDOW_BORDER_PIXEL_SIZE, height, 0);
		if (!border_right_graphics_info)
		{
			res = -ENOMEM;
			goto out;
		}

		struct graphics_info *border_bottom_graphics_info = graphics_info_create_relative(root_graphics_info, 0, total_window_height_bounds - WINDOW_BORDER_PIXEL_SIZE, width, WINDOW_BORDER_PIXEL_SIZE, 0);
		if (!border_bottom_graphics_info)
		{
			res = -ENOMEM;
			goto out;
		}
	}

	struct graphics_info *window_graphics_info = graphics_info_create_relative(root_graphics_info, window_body_width_offset, window_body_height_offset, width, height, 0);
	if (!window_graphics_info)
	{
		res = -ENOMEM;
		goto out;
	}

	window->graphics = window_graphics_info;
	if (!(flags & WINDOW_FLAG_BORDERLESS))
	{
		struct framebuffer_pixel title_bar_font_color = {0};
		title_bar_font_color.red = 0xff;
		title_bar_font_color.green = 0xff;
		title_bar_font_color.blue = 0xff;

		window->title_bar_terminal = terminal_create(title_bar_graphics_info, 0, 0, total_window_width_bounds, WINDOW_TITLE_BAR_HEIGHT, font, title_bar_font_color, 0);
		if (!window->title_bar_terminal)
		{
			res = -ENOMEM;
			goto out;
		}
	}

	struct framebuffer_pixel pixel_color = {0};
	pixel_color.red = 0xc0;
	window->terminal = terminal_create(window_graphics_info, 0, 0, width, height, font, pixel_color, TERMINAL_FLAG_BACKSPACE_ALLOWED);
	if (!window->terminal)
	{
		res = -ENOMEM;
		goto out;
	}

	struct framebuffer_pixel bg_color = {0};
	bg_color.red = 0xff;
	bg_color.green = 0xff;
	bg_color.blue = 0xff;
	terminal_draw_rect(window->terminal, 0, 0, width, height, bg_color);

	// save background of the terminal in case of backspaces
	terminal_background_save(window->terminal);

	if (flags & WINDOW_FLAG_BACKGROUND_TRANSPARENT)
	{
		terminal_transparency_key_set(window->terminal, bg_color);
	}

	if (!(flags & WINDOW_FLAG_BORDERLESS))
	{
		size_t icon_pos_x = window->title_bar_terminal->bounds.width - close_icon->width - (close_icon->width / 2);
		size_t icon_pos_y = (window->title_bar_terminal->bounds.height / 2) - (close_icon->height / 2);

		window->title_bar_components.close_button.x = icon_pos_x;
		window->title_bar_components.close_button.y = icon_pos_y;
		window->title_bar_components.close_button.width = close_icon->width;
		window->title_bar_components.close_button.height = close_icon->height;

		struct framebuffer_pixel title_bar_bg_color = {0};
		window_draw_title_bar(window, title_bar_bg_color);

		struct framebuffer_pixel border_color = {0};
		graphics_draw_rect(border_left_graphics_info, 0, 0, border_left_graphics_info->width, border_left_graphics_info->height, border_color);
		graphics_draw_rect(border_right_graphics_info, 0, 0, border_right_graphics_info->width, border_right_graphics_info->height, border_color);
		graphics_draw_rect(border_bottom_graphics_info, 0, 0, border_bottom_graphics_info->width, border_bottom_graphics_info->height, border_color);
	}

	vector_push(windows_vector, &window);

	size_t child_count = vector_count(window->root_graphics->children);
	window_set_z_index(window, child_count + 1);

	// register window event handlers
	window_event_handler_register(window, window_event_handler);

	window_focus(window);

	// redraw all graphics including our new window
	graphics_redraw_all();

out:
	if (res < 0)
	{
		if (window)
		{
			if (window->terminal)
			{
				terminal_free(window->terminal);
				window->terminal = NULL;
			}

			if (window->title_bar_terminal)
			{
				terminal_free(window->title_bar_terminal);
				window->title_bar_terminal = NULL;
			}

			vector_pop_element(windows_vector, &window, sizeof(struct window *));
			kfree(window);
			window = NULL;
		}
	}

	return window;
}
