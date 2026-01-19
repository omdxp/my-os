#include "file.h"
#include "myos.h"
#include <stddef.h>

int fopen(const char *filename, const char *mode)
{
	return myos_fopen(filename, mode);
}

void fclose(int fd)
{
	myos_fclose((size_t)fd);
}