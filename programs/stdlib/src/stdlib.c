#include "stdlib.h"
#include "myos.h"

void *malloc(size_t size)
{
	return myos_malloc(size);
}

void free(void *ptr)
{
}
