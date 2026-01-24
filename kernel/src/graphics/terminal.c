#include "terminal.h"

struct vector *terminal_vector = NULL;

inline static size_t terminal_abs_x_for_next_character(struct terminal *term)
{
	return term->bounds.abs_x + (term->text.col * term->font->bits_width_per_character);
}

inline static size_t terminal_abs_y_for_next_character(struct terminal *term)
{
	return term->bounds.abs_y + (term->text.row * term->font->bits_height_per_character);
}

void terminal_system_setup()
{
	terminal_vector = vector_new(sizeof(struct terminal *), 4, 0);
}

struct terminal *terminal_create(struct graphics_info *graphics_info, int starting_x, int starting_y, size_t width, size_t height, struct font *font, struct framebuffer_pixel font_color, int flags)
{
	int res = 0;
	if (!graphics_info || !font)
	{
		return NULL;
	}

	if (starting_x < 0 || starting_y < 0 || width == 0 || height == 0 || starting_x >= graphics_info->horizontal_resolution || starting_y >= graphics_info->vertical_resolution || (size_t)starting_x + width > graphics_info->horizontal_resolution || (size_t)starting_y + height > graphics_info->vertical_resolution)
	{
		return NULL;
	}

	struct terminal *term = kzalloc(sizeof(struct terminal));
	if (!term)
	{
		res = -ENOMEM;
		goto out;
	}

	term->graphics_info = graphics_info;
	term->terminal_background = NULL; // for now, no background
	term->text.row = 0;
	term->text.col = 0;
	term->bounds.abs_x = (size_t)starting_x;
	term->bounds.abs_y = (size_t)starting_y;
	term->bounds.width = width;
	term->bounds.height = height;
	term->font = font;
	term->font_color = font_color;
	term->flags = flags;

	// save the background of the screen where the terminal will be drawn
	terminal_background_save(term);

	res = vector_push(terminal_vector, &term);
	if (res < 0)
	{
		kfree(term);
		term = NULL;
		goto out;
	}

out:
	if (res < 0)
	{
		terminal_free(term);
		term = NULL;
	}

	return term;
}

void terminal_free(struct terminal *term)
{
	if (term->terminal_background)
	{
		kfree(term->terminal_background);
		term->terminal_background = NULL;
	}
	vector_pop_element(terminal_vector, &term, sizeof(term));
	kfree(term);
}

struct terminal *terminal_get_at_screen_position(size_t x, size_t y, struct terminal *ignore_terminal)
{
	int res = 0;
	struct terminal *found_terminal = NULL;
	size_t total_children = vector_count(terminal_vector);
	for (size_t i = 0; i < total_children; i++)
	{
		struct terminal *term = NULL;
		res = vector_at(terminal_vector, i, &term, sizeof(term));
		if (res < 0)
		{
			goto out;
		}

		if (term == ignore_terminal)
		{
			continue;
		}

		if (x >= term->bounds.abs_x && x <= term->bounds.width && y >= term->bounds.abs_y && y <= term->bounds.height)
		{
			found_terminal = term;
			break;
		}
	}

out:
	if (res < 0)
	{
		found_terminal = NULL;
	}
	return found_terminal;
}

void terminal_background_save(struct terminal *term)
{
	size_t width = term->bounds.width;
	size_t height = term->bounds.height;
	size_t total_pixels = width * height;
	size_t buffer_size = total_pixels * sizeof(struct framebuffer_pixel);

	if (!term->terminal_background)
	{
		term->terminal_background = kzalloc(buffer_size);
		if (!term->terminal_background)
		{
			// ENOMEM
			return;
		}
	}

	// copy the pixels from the graphics_info framebuffer to the terminal background buffer
	memcpy(term->terminal_background, term->graphics_info->pixels, buffer_size);
}

static void terminal_handle_new_line(struct terminal *term)
{
	term->text.row++;
	size_t total_rows_per_term = terminal_total_rows(term);

	// wrap vertically if needed
	if (term->text.row >= total_rows_per_term)
	{
		term->text.row = 0;
	}

	// reset column to 0
	term->text.col = 0;
}

static void terminal_update_position_after_draw(struct terminal *term)
{
	term->text.col++;
	size_t total_cols_per_row = terminal_total_columns(term);
	size_t total_rows_per_term = terminal_total_rows(term);

	// wrap horizontally if needed
	if (term->text.col >= total_cols_per_row)
	{
		term->text.col = 0;
		term->text.row++;
	}

	// wrap vertically if needed
	if (term->text.row >= total_rows_per_term)
	{
		term->text.col = 0;
		term->text.row = 0;
	}
}

int terminal_cursor_set(struct terminal *term, size_t row, size_t col)
{
	int res = 0;
	size_t total_cols_per_row = terminal_total_columns(term);
	size_t total_rows_per_term = terminal_total_rows(term);
	if (col < 0 || col >= total_cols_per_row || row < 0 || row >= total_rows_per_term)
	{
		res = -EINVARG;
		goto out;
	}

	term->text.col = col;
	term->text.row = row;
out:
	return res;
}

int terminal_cursor_row(struct terminal *term)
{
	return term->text.row;
}

int terminal_cursor_column(struct terminal *term)
{
	return term->text.col;
}

int terminal_total_columns(struct terminal *term)
{
	return term->bounds.width / term->font->bits_width_per_character;
}

int terminal_total_rows(struct terminal *term)
{
	return term->bounds.height / term->font->bits_height_per_character;
}

static bool terminal_bounds_check(struct terminal *term, size_t abs_x, size_t abs_y)
{
	size_t starting_x = term->bounds.abs_x;
	size_t starting_y = term->bounds.abs_y;
	size_t ending_x = starting_x + term->bounds.width;
	size_t ending_y = starting_y + term->bounds.height;
	return (abs_x >= starting_x && abs_x <= ending_x && abs_y >= starting_y && abs_y <= ending_y);
}

void terminal_restore_background(struct terminal *term, size_t sx, size_t sy, size_t width, size_t height)
{
	for (size_t x = 0; x < width; x++)
	{
		for (size_t y = 0; y < height; y++)
		{
			size_t abs_x = term->bounds.abs_x + sx + x;
			size_t abs_y = term->bounds.abs_y + sy + y;

			// check if there was an overflow
			if (abs_x > term->bounds.width || abs_y > term->bounds.height)
			{
				continue;
			}

			struct framebuffer_pixel background_pixel = {0};
			background_pixel = term->terminal_background[y * term->bounds.width + x];
			graphics_draw_pixel(term->graphics_info, abs_x, abs_y, background_pixel);
		}
	}

	size_t abs_x = term->bounds.abs_x + sx;
	size_t abs_y = term->bounds.abs_y + sy;
	graphics_redraw_graphics_to_screen(term->graphics_info, abs_x, abs_y, width, height);
}

int terminal_backspace(struct terminal *term)
{
	int res = 0;
	if (!(term->flags & TERMINAL_FLAG_BACKSPACE_ALLOWED))
	{
		return 0;
	}

	int total_rows = terminal_total_rows(term);
	int total_cols = terminal_total_columns(term);
	int current_col = terminal_cursor_column(term);
	int current_row = terminal_cursor_row(term);

	current_col--;
	if (current_col < 0)
	{
		current_col = total_cols - 1;
		current_row--;
	}

	if (current_row < 0)
	{
		current_row = total_rows - 1;
		current_col = 0;
	}

	if (current_col >= total_cols)
	{
		current_col = 0;
		current_row++;
	}

	if (current_row >= total_rows)
	{
		current_row = 0;
		current_col = 0;
	}

	// update the cursor position
	terminal_cursor_set(term, current_row, current_col);

	size_t rel_x = term->text.col * term->font->bits_width_per_character;
	size_t rel_y = term->text.row * term->font->bits_height_per_character;
	terminal_restore_background(term, rel_x, rel_y, term->font->bits_width_per_character, term->font->bits_height_per_character);
	return res;
}

int terminal_write(struct terminal *term, int c)
{
	if (c == '\n')
	{
		terminal_handle_new_line(term);
		return 0;
	}

	if (c == 0x08 && (term->flags & TERMINAL_FLAG_BACKSPACE_ALLOWED))
	{
		return terminal_backspace(term);
	}

	size_t abs_x = terminal_abs_x_for_next_character(term);
	size_t abs_y = terminal_abs_y_for_next_character(term);

	font_draw(term->graphics_info, term->font, abs_x, abs_y, c, term->font_color);
	terminal_update_position_after_draw(term);
	return 0;
}

int terminal_pixel_set(struct terminal *term, size_t x, size_t y, struct framebuffer_pixel pixel_color)
{
	int res = 0;
	size_t abs_x = term->bounds.abs_x + x;
	size_t abs_y = term->bounds.abs_y + y;

	if (!terminal_bounds_check(term, abs_x, abs_y))
	{
		res = -EINVARG;
		goto out;
	}

	graphics_draw_pixel(term->graphics_info, abs_x, abs_y, pixel_color);
out:
	return res;
}

void terminal_transparency_key_set(struct terminal *term, struct framebuffer_pixel pixel_color)
{
	graphics_transparency_key_set(term->graphics_info, pixel_color);
}

void terminal_transparency_key_remove(struct terminal *term)
{
	graphics_transparency_key_remove(term->graphics_info);
}

void terminal_ignore_color(struct terminal *term, struct framebuffer_pixel pixel_color)
{
	graphics_ignore_color(term->graphics_info, pixel_color);
}

void terminal_ignore_color_finish(struct terminal *term)
{
	graphics_ignore_color_finish(term->graphics_info);
}

int terminal_draw_image(struct terminal *term, uint32_t x, uint32_t y, struct image *image)
{
	int res = 0;
	size_t abs_x = term->bounds.abs_x + x;
	size_t abs_y = term->bounds.abs_y + y;

	if (!terminal_bounds_check(term, abs_x, abs_y))
	{
		res = -EINVARG;
		goto out;
	}

	graphics_draw_image(term->graphics_info, image, abs_x, abs_y);
out:
	return res;
}

int terminal_draw_rect(struct terminal *term, uint32_t x, uint32_t y, size_t width, size_t height, struct framebuffer_pixel pixel_color)
{
	int res = 0;
	size_t abs_x = term->bounds.abs_x + x;
	size_t abs_y = term->bounds.abs_y + y;

	if (!terminal_bounds_check(term, abs_x, abs_y))
	{
		res = -EINVARG;
		goto out;
	}

	graphics_draw_rect(term->graphics_info, abs_x, abs_y, width, height, pixel_color);
out:
	return res;
}

int terminal_print(struct terminal *term, const char *str)
{
	int res = 0;
	while (*str)
	{
		res = terminal_write(term, *str);
		if (res < 0)
		{
			break;
		}
		str++;
	}

	return res;
}
