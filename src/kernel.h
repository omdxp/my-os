#pragma once

#define VGA_WIDTH 80
#define VGA_HEIGHT 20

#define MYOS_MAX_PATH 108

void kernel_main();
void print(const char *str);
void terminal_writechar(char c, char color);
void panic(const char *msg);
void kernel_page();
void kernel_registers();
struct paging_desc *kernel_desc();

#define ERROR(value) (void *)((intptr_t)(value))
#define ERROR_I(value) (int)((intptr_t)(value))
#define ISERR(value) ((int)value < 0)
