#pragma once
#include "idt/idt.h"

void *isr80h_command10_fopen(struct interrupt_frame *frame);
void *isr80h_command11_fclose(struct interrupt_frame *frame);
void *isr80h_command12_fread(struct interrupt_frame *frame);
void *isr80h_command13_fseek(struct interrupt_frame *frame);