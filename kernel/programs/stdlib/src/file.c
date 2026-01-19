#include "file.h"
#include "myos.h"

int fopen(const char *filename, const char *mode)
{
	return myos_fopen(filename, mode);
}

void fclose(int fd)
{
	myos_fclose((size_t)fd);
}

int fread(void *buffer, size_t size, size_t count, long fd)
{
	return (int)myos_fread(buffer, size, count, fd);
}

int fseek(long fd, int offset, int whence)
{
	return (int)myos_fseek(fd, offset, whence);
}