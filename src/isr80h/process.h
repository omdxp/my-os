#pragma once

struct interrupt_frame;

void *isr80h_comman6_process_load_start(struct interrupt_frame *frame);
void *isr80h_comman7_invoke_system_command(struct interrupt_frame *frame);
void *isr80h_command8_get_program_arguments(struct interrupt_frame *frame);
void *isr80h_command9_exit(struct interrupt_frame *frame);
