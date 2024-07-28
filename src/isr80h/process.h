#pragma once

struct interrupt_frame;

void *isr80h_comman6_process_load_start(struct interrupt_frame *frame);
