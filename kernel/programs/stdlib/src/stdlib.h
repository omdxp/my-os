#pragma once

#include <stddef.h>

void *malloc(size_t size);
void *calloc(size_t n_memb, size_t size);
void *realloc(void *ptr, size_t new_size);
void free(void *ptr);
char *itoa(int i);
char *utoa(unsigned int i);