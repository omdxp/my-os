#pragma once

#include <stdint.h>
#include <stddef.h>

void kheap_init();
void *kmalloc(size_t size);
void kfree(void *ptr);
void *kzalloc(size_t size);
void *kpalloc(size_t size);
void *kpzalloc(size_t size);
void kheap_post_paging();
