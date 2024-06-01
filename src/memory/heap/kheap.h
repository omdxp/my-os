#pragma once

#include <stdint.h>
#include <stddef.h>

void kheap_init();
void *kmalloc(size_t size);
void kfree(void *ptr);
