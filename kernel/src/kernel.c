#include "kernel.h"
#include <stddef.h>
#include <stdint.h>
#include "idt/idt.h"
#include "io/io.h"
#include "memory/heap/kheap.h"
#include "memory/heap/heap.h"
#include "memory/memory.h"
#include "memory/paging/paging.h"
#include "task/task.h"
#include "task/process.h"
#include "disk/disk.h"
#include "fs/file.h"
#include "fs/pparser.h"
#include "string/string.h"
#include "disk/streamer.h"
#include "gdt/gdt.h"
#include "task/tss.h"
#include "config.h"
#include "status.h"
#include "isr80h/isr80h.h"
#include "keyboard/keyboard.h"

uint16_t *video_mem = 0;
uint16_t terminal_row = 0;
uint16_t terminal_col = 0;

uint16_t terminal_make_char(char c, char color)
{
	return (color << 8) | c;
}

void terminal_putchar(size_t x, size_t y, char c, char color)
{
	video_mem[(y * VGA_WIDTH) + x] = terminal_make_char(c, color);
}

void terminal_backspace()
{
	if (terminal_row == 0 && terminal_col == 0)
	{
		return;
	}

	if (terminal_col == 0)
	{
		terminal_row -= 1;
		terminal_col = VGA_WIDTH;
	}

	terminal_col -= 1;
	terminal_writechar(' ', 15);
	terminal_col -= 1;
}

void terminal_writechar(char c, char color)
{
	if (c == '\n')
	{
		terminal_col = 0;
		terminal_row += 1;
		return;
	}

	if (c == 0x08) // backspace
	{
		terminal_backspace();
		return;
	}

	terminal_putchar(terminal_col, terminal_row, c, color);
	terminal_col += 1;
	if (terminal_col >= VGA_WIDTH)
	{
		terminal_col = 0;
		terminal_row += 1;
	}
}

void terminal_init()
{
	video_mem = (uint16_t *)(0xB8000);
	terminal_row = 0;
	terminal_col = 0;
	for (size_t y = 0; y < VGA_HEIGHT; y++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			terminal_putchar(x, y, ' ', 0);
		}
	}
}

void print(const char *str)
{
	size_t len = strlen(str);
	for (size_t i = 0; i < len; i++)
	{
		terminal_writechar(str[i], 15);
	}
}

// static struct paging_4gb_chunk *kernel_chunk = 0;

void panic(const char *msg)
{
	print(msg);
	while (42)
		;
}

struct tss tss;
extern struct gdt_entry gdt[];

// page descriptor for 64-bit paging
struct paging_desc *kernel_paging_desc = 0;

void kernel_page()
{
	kernel_registers();
	paging_switch(kernel_paging_desc);
}

struct paging_desc *kernel_desc()
{
	return kernel_paging_desc;
}

void kernel_main()
{
	terminal_init();
	print("Welcome to MyOS on 64-bit mode!\n");

	print("Total accessible memory: ");
	print(itoa((int)e820_total_accessible_memory()));
	print(" bytes\n");

	kheap_init();
	char *ptr = kmalloc(50);
	ptr[0] = 'H';
	ptr[1] = 'e';
	ptr[2] = 'l';
	ptr[3] = 'l';
	ptr[4] = 'o';
	ptr[5] = ',';
	ptr[6] = ' ';
	ptr[7] = 'W';
	ptr[8] = 'o';
	ptr[9] = 'r';
	ptr[10] = 'l';
	ptr[11] = 'd';
	ptr[12] = '!';
	ptr[13] = '\n';
	ptr[14] = '\0';
	print(ptr);

	kernel_paging_desc = paging_desc_new(PAGING_MAP_LEVEL_4);
	if (!kernel_paging_desc)
	{
		panic("kernel_main: paging_desc_new failed\n");
	}

	paging_map_e820_memory_regions(kernel_paging_desc);

	paging_switch(kernel_paging_desc);

	kheap_post_paging();

	// initialize the interrupt descriptor table
	idt_init();

	// initialize file systems
	fs_init();

	// initialize disks
	disk_search_and_init();

	// allocate a 1MB stack for the kernel IDT
	size_t stack_size = 1024 * 1024;
	void *megabyte_stack_tss_end = kzalloc(stack_size);
	void *megabyte_stack_tss_start = (void *)(((uintptr_t)megabyte_stack_tss_end) + stack_size);

	// block first page to catch stack overflows
	paging_map(kernel_desc(), megabyte_stack_tss_end, megabyte_stack_tss_end, 0);

	// setup tss
	memset(&tss, 0x00, sizeof(tss));
	tss.rsp0 = (uint64_t)megabyte_stack_tss_start;
	tss.iopb_offset = sizeof(tss); // no I/O bitmap

	struct tss_desc_64 *tss_desc = (struct tss_desc_64 *)&gdt[KERNEL_LONG_MODE_TSS_GDT_INDEX];
	gdt_set_tss(tss_desc, &tss, sizeof(tss) - 1, TSS_DESCRIPTOR_TYPE, 0);

	// load tss
	tss_load(KERNEL_LONG_MODE_TSS_SELECTOR);
	print("TSS loaded\n");

	// register kernel commands
	isr80h_register_commands();
	print("isr80h commands registered\n");

	// initialize keyboard
	keyboard_init();
	print("Keyboard initialized\n");

	// load program
	struct process *process = 0;
	int res = process_load_switch("@:/shell.elf", &process);
	if (res != MYOS_ALL_OK)
	{
		print("Error code: ");
		print(itoa(-res));
		print("\n");
		panic("Failed to load shell.elf\n");
	}
	print("shell.elf loaded\n");

	// drop to user land
	task_run_first_ever_task();

	while (1)
		;
}
