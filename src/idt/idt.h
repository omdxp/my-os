#pragma once

#include <stdint.h>

struct interrupt_frame;
typedef void *(*ISR80H_COMMAND)(struct interrupt_frame *frame);
typedef void (*INTERRUPT_CALLBACK_FUNCTION)();

// 64 bit idt descriptor
struct idt_desc
{
	uint16_t offset_1; // offset bits 0 - 15
	uint16_t selector; // selector that in our GDT
	uint8_t ist;	   // bits 0-2 hold Interrupt Stack Table offset, rest of bits zero.
	uint8_t type_attr; // descriptor type and attributes
	uint16_t offset_2; // offset bits 16-31
	uint32_t offset_3; // offset bits 32-63
	uint32_t reserved; // reserved set to 0
} __attribute__((packed));

struct idtr_desc
{
	uint16_t limit; // size of descriptor table - 1
	uint64_t base;	// base address of the start of the interrupt descriptor table
} __attribute__((packed));

struct interrupt_frame
{
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rbp;
	uint64_t reserved;
	uint64_t rbx;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rax;
	uint64_t ip;
	uint64_t cs;
	uint64_t flags;
	uint64_t rsp;
	uint64_t ss;
} __attribute__((packed));

// struct idt_desc
// {
// 	uint16_t offset_1; // offset bits 0 - 15
// 	uint16_t selector; // selector that in our GDT
// 	uint8_t zero;	   // unused set to 0
// 	uint8_t type_attr; // descriptor type and attributes
// 	uint16_t offset_2; // offset bits 16-31
// } __attribute__((packed));

// struct idtr_desc
// {
// 	uint16_t limit; // size of descriptor table - 1
// 	uint32_t base;	// base address of the start of the interrupt descriptor table
// } __attribute__((packed));

// struct interrupt_frame
// {
// 	uint32_t edi;
// 	uint32_t esi;
// 	uint32_t ebp;
// 	uint32_t reserved;
// 	uint32_t ebx;
// 	uint32_t edx;
// 	uint32_t ecx;
// 	uint32_t eax;
// 	uint32_t ip;
// 	uint32_t cs;
// 	uint32_t flags;
// 	uint32_t esp;
// 	uint32_t ss;
// } __attribute__((packed));

void idt_init();
void enable_interrupts();
void disable_interrupts();
void isr80h_register_command(int command_id, ISR80H_COMMAND command);
int idt_register_interrupt_callback(int interrupt, INTERRUPT_CALLBACK_FUNCTION interrupt_callback);
