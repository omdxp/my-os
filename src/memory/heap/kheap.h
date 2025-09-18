#pragma once

#include <stdint.h>
#include <stddef.h>

void kheap_init(size_t initial_size);
void *kmalloc(size_t size);
void kfree(void *ptr);
void *kzalloc(size_t size);
struct heap *kheap_get();
