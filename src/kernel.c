#include "kernel.h"
#include <stddef.h>
#include <stdint.h>
#include "idt/idt.h"
#include "io/io.h"
#include "memory/heap/kheap.h"
#include "memory/paging/paging.h"
#include "disk/disk.h"
#include "fs/file.h"
#include "fs/pparser.h"
#include "string/string.h"
#include "disk/streamer.h"

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

void terminal_writechar(char c, char color)
{
	if (c == '\n')
	{
		terminal_col = 0;
		terminal_row += 1;
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

static struct paging_4gb_chunk *kernel_chunk = 0;

void panic(const char *msg)
{
	print(msg);
	while (42)
	{
	}
}

void kernel_main()
{
	terminal_init();
	print("Hello, World!\n");

	// initialize the heap
	kheap_init();

	// initialize filesystems
	fs_init();

	// search and initialize disks
	disk_search_and_init();

	// initialize the interrupt descriptor table
	idt_init();

	// setup paging
	kernel_chunk = paging_new_4gb(PAGING_IS_WRITEABLE | PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL);

	// switch to kernel paging chunk
	paging_switch(paging_4gb_chunk_get_directory(kernel_chunk));

	// enable paging
	enable_paging();

	// enable interrupts
	enable_interrupts();

	int fd = fopen("0:/hello.txt", "r");
	if (fd)
	{
		struct file_stat s;
		fstat(fd, &s);
		fclose(fd);
		print("file closed\n");
	}
	while (1)
	{
	}
}
