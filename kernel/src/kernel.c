#include "kernel.h"
#include <stddef.h>
#include <stdint.h>
#include "idt/idt.h"
#include "io/io.h"
#include "io/tsc.h"
#include "memory/heap/kheap.h"
#include "memory/heap/heap.h"
#include "memory/memory.h"
#include "memory/paging/paging.h"
#include "task/task.h"
#include "task/process.h"
#include "disk/disk.h"
#include "disk/gpt.h"
#include "fs/file.h"
#include "fs/pparser.h"
#include "string/string.h"
#include "disk/streamer.h"
#include "gdt/gdt.h"
#include "graphics/graphics.h"
#include "graphics/image/image.h"
#include "graphics/font.h"
#include "graphics/terminal.h"
#include "graphics/window.h"
#include "task/tss.h"
#include "config.h"
#include "status.h"
#include "isr80h/isr80h.h"
#include "keyboard/keyboard.h"
#include "mouse/mouse.h"

struct terminal *system_terminal = NULL;

void terminal_writechar(char c, char color)
{
	if (!system_terminal)
	{
		return;
	}

	terminal_write(system_terminal, c);
}

void print(const char *str)
{
	size_t len = strlen(str);
	for (size_t i = 0; i < len; i++)
	{
		terminal_writechar(str[i], 15);
	}
}

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

// defined in kernel.asm
extern struct graphics_info default_graphics_info;

void kernel_main()
{
	// initialize kernel heap
	kheap_init();

	// setup paging
	kernel_paging_desc = paging_desc_new(PAGING_MAP_LEVEL_4);
	if (!kernel_paging_desc)
	{
		panic("kernel_main: paging_desc_new failed\n");
	}

	paging_map_e820_memory_regions(kernel_paging_desc);

	paging_switch(kernel_paging_desc);
	kheap_post_paging();

	// setup graphics
	graphics_setup(&default_graphics_info);
	struct graphics_info *screen_info = graphics_screen_info();

	// initialize the interrupt descriptor table
	idt_init();

	// initialize file systems
	fs_init();

	// initialize disks
	disk_search_and_init();

	// initialize GPT drives
	gpt_init();

	// initialize font system
	font_system_init();

	// setup terminal system
	terminal_system_setup();

	// initialize mouse system
	mouse_system_init();

	// initialize window system
	window_system_initialize();

	// load static mouse drivers
	mouse_system_load_static_drivers();

	// initlialize graphics stage 2
	graphics_setup_stage2(&default_graphics_info);

	// initialize window system stage 2
	window_system_initialize_stage2();

	struct font *font = font_get_system_font();
	if (!font)
	{
		panic("kernel_main: font_get_system_font failed\n");
	}
	struct framebuffer_pixel font_color = {255, 255, 255, 0};
	system_terminal = terminal_create(screen_info,
									  0, 0,
									  screen_info->horizontal_resolution,
									  screen_info->vertical_resolution,
									  font,
									  font_color,
									  TERMINAL_FLAG_BACKSPACE_ALLOWED);
	if (!system_terminal)
	{
		panic("kernel_main: terminal_create failed\n");
	}

	// allocate a 1MB stack for the kernel IDT
	size_t stack_size = 1024 * 1024;
	void *megabyte_stack_tss_end = kzalloc(stack_size);
	void *megabyte_stack_tss_start = (void *)(((uintptr_t)megabyte_stack_tss_end) + stack_size);

	// block first page to catch stack overflows
	paging_map(kernel_desc(), megabyte_stack_tss_end, megabyte_stack_tss_end, 0);
	print("Stack overflow page blocked\n");

	// setup tss
	memset(&tss, 0x00, sizeof(tss));
	tss.rsp0 = (uint64_t)megabyte_stack_tss_start;
	tss.iopb_offset = sizeof(tss); // no I/O bitmap

	struct tss_desc_64 *tss_desc = (struct tss_desc_64 *)&gdt[KERNEL_LONG_MODE_TSS_GDT_INDEX];
	gdt_set_tss(tss_desc, &tss, sizeof(tss) - 1, TSS_DESCRIPTOR_TYPE, 0);
	print("TSS set up\n");

	// load tss
	tss_load(KERNEL_LONG_MODE_TSS_SELECTOR);
	print("TSS loaded\n");

	// initialize process system
	process_system_init();
	print("Process system initialized\n");

	// register kernel commands
	isr80h_register_commands();
	print("isr80h commands registered\n");

	// initialize keyboard
	keyboard_init();
	print("Keyboard initialized\n");

	// load background image
	// struct image *img = graphics_image_load("@:/backgrnd.bmp");
	// graphics_draw_image(NULL, img, 0, 0);
	// graphics_redraw_all();

	struct window *win = window_create(screen_info, NULL, "MyOS Window", 50, 50, 400, 300, 0, -1);
	if (win)
		;

	// enable interrupts
	enable_interrupts();
	print("Interrupts enabled\n");
	while (1)
		;

	// load program
	struct process *process = 0;
	int res = process_load_switch("@:/blank.elf", &process);
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
