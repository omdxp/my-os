#include "window.h"
#include "graphics/graphics.h"
#include "graphics/window.h"
#include "isr80h/graphics.h"
#include "task/process.h"
#include "task/task.h"
#include "task/userlandptr.h"
#include "status.h"
#include "kernel.h"

struct window *isr80h_window_from_process_window_virt(void *proc_win_virt_addr)
{
	struct process_window *proc_win = process_window_get_from_user_window(task_current()->process, (struct process_userspace_window *)proc_win_virt_addr);
	if (!proc_win)
	{
		return NULL;
	}

	return proc_win->kernel_win;
}

void *isr80h_command16_window_create(struct interrupt_frame *frame)
{
	int res = 0;
	struct process_window *proc_win = NULL;
	void *window_title_user_ptr = task_get_stack_item(task_current(), 0);
	char win_title[WINDOW_MAX_TITLE_LENGTH];
	res = copy_string_from_task(task_current(), window_title_user_ptr, win_title, sizeof(win_title));
	if (res < 0)
	{
		goto out;
	}

	int win_width = (int)(uintptr_t)task_get_stack_item(task_current(), 1);
	int win_height = (int)(uintptr_t)task_get_stack_item(task_current(), 2);
	int win_flags = (int)(uintptr_t)task_get_stack_item(task_current(), 3);
	int win_id = (int)(uintptr_t)task_get_stack_item(task_current(), 4);

	proc_win = process_window_create(task_current()->process, win_title, win_width, win_height, win_flags, win_id);
	if (!proc_win)
	{
		res = -EINVARG;
		goto out;
	}

out:
	if (res < 0)
	{
		if (proc_win)
		{
			// TODO: free the window
			proc_win = NULL;
		}

		return NULL;
	}

	return proc_win->user_win;
}

void *isr80h_command17_sysout_to_window(struct interrupt_frame *frame)
{
	void *user_win_ptr = task_get_stack_item(task_current(), 0);
	if (user_win_ptr)
	{
		struct process_window *proc_win = process_window_get_from_user_window(task_current()->process, (struct process_userspace_window *)user_win_ptr);
		if (proc_win)
		{
			process_set_sysout_window(task_current()->process, proc_win);
		}
	}

	return 0;
}

void *isr80h_command18_get_window_event(struct interrupt_frame *frame)
{
	int res = 0;
	struct window_event_userland *win_event_out = NULL;

	void *win_event_out_virt_addr = task_get_stack_item(task_current(), 0);
	if (!win_event_out_virt_addr)
	{
		res = -EINVARG;
		goto out;
	}

	win_event_out = task_virtual_addr_to_phys(task_current(), win_event_out_virt_addr);
	if (!win_event_out)
	{
		res = -EINVARG;
		goto out;
	}

	struct window_event win_event_kern = {0};
	res = process_pop_window_event(task_current()->process, &win_event_kern);
	if (res < 0)
	{
		goto out;
	}

	window_event_to_userland(&win_event_kern, win_event_out);

out:
	return (void *)(uintptr_t)res;
}

void *isr80h_command19_window_graphics_get(struct interrupt_frame *frame)
{
	void *user_win_ptr = task_get_stack_item(task_current(), 0);
	if (!user_win_ptr)
	{
		return NULL;
	}

	struct window *kern_window = isr80h_window_from_process_window_virt(user_win_ptr);
	if (!kern_window)
	{
		return NULL;
	}

	struct graphics_info *graphics = kern_window->graphics;
	struct userland_graphics *userland_graphics = NULL;
	userland_graphics = isr80h_graphics_make_userland_metadata(task_current()->process, graphics);
	if (!userland_graphics)
	{
		return NULL;
	}

	// userland owns this pointer and must free it when done
	return (void *)userland_graphics;
}

void *isr80h_command21_window_redraw(struct interrupt_frame *frame)
{
	void *user_win_ptr = task_get_stack_item(task_current(), 0);
	if (!user_win_ptr)
	{
		return NULL;
	}

	struct window *kern_window = isr80h_window_from_process_window_virt(user_win_ptr);
	if (!kern_window)
	{
		return NULL;
	}

	window_redraw(kern_window);
	return NULL;
}

void *isr80h_command23_window_redraw_region(struct interrupt_frame *frame)
{
	long rect_x = (long)(uintptr_t)task_get_stack_item(task_current(), 0);
	long rect_y = (long)(uintptr_t)task_get_stack_item(task_current(), 1);
	long rect_width = (long)(uintptr_t)task_get_stack_item(task_current(), 2);
	long rect_height = (long)(uintptr_t)task_get_stack_item(task_current(), 3);
	void *user_win_ptr = task_get_stack_item(task_current(), 4);
	if (!user_win_ptr)
	{
		return ERROR(-EINVARG);
	}

	struct window *kern_window = isr80h_window_from_process_window_virt(user_win_ptr);
	if (!kern_window)
	{
		return ERROR(-EINVARG);
	}

	window_redraw_body_region(kern_window, rect_x, rect_y, rect_width, rect_height);
	return NULL;
}