#pragma once

struct interrupt_frame;
void *isr80h_command16_window_create(struct interrupt_frame *frame);
void *isr80h_command17_sysout_to_window(struct interrupt_frame *frame);