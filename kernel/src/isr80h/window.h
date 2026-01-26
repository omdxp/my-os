#pragma once

enum
{
	ISR80h_WINDOW_UPDATE_TITLE,
};

struct interrupt_frame;
void *isr80h_command16_window_create(struct interrupt_frame *frame);
void *isr80h_command17_sysout_to_window(struct interrupt_frame *frame);
void *isr80h_command18_get_window_event(struct interrupt_frame *frame);
void *isr80h_command19_window_graphics_get(struct interrupt_frame *frame);
void *isr80h_command21_window_redraw(struct interrupt_frame *frame);
void *isr80h_command23_window_redraw_region(struct interrupt_frame *frame);
void *isr80h_command24_update_window(struct interrupt_frame *frame);