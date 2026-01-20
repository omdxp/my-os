
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef unsigned int FILE_STAT_FLAGS;

struct file_stat
{
	FILE_STAT_FLAGS flags;
	uint32_t filesize;
};

int fopen(const char *filename, const char *mode);
void fclose(int fd);
int fread(void *buffer, size_t size, size_t count, long fd);
int fseek(long fd, int offset, int whence);
int fstat(long fd, struct file_stat *filestat_out);