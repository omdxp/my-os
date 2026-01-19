#pragma once
#include <stddef.h>

int fopen(const char *filename, const char *mode);
void fclose(int fd);
int fread(void *buffer, size_t size, size_t count, long fd);
int fseek(long fd, int offset, int whence);