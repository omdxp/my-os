#pragma once

#include <stddef.h>

void print(const char *filename);
int getkey();
void *myos_malloc(size_t size);
void myos_free(void *ptr);
