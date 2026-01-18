#include "file.h"
#include "myos.h"

int fopen(const char *filename, const char *mode)
{
	return myos_fopen(filename, mode);
}