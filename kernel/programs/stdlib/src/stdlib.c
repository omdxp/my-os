#include "stdlib.h"
#include "myos.h"

char *itoa(int i)
{
	static char text[12];
	int loc = 11;
	text[11] = 0;
	char neg = 1;
	if (i >= 0)
	{
		neg = 0;
		i = -i;
	}

	while (i)
	{
		text[--loc] = '0' - (i % 10);
		i /= 10;
	}

	if (loc == 11)
	{
		text[--loc] = '0';
	}

	if (neg)
	{
		text[--loc] = '-';
	}

	return &text[loc];
}

char *utoa(unsigned int i)
{
	static char text[11];
	int loc = 10;
	text[10] = 0;

	while (i)
	{
		text[--loc] = '0' + (i % 10);
		i /= 10;
	}

	if (loc == 10)
	{
		text[--loc] = '0';
	}

	return &text[loc];
}

void *malloc(size_t size)
{
	return myos_malloc(size);
}

void free(void *ptr)
{
	return myos_free(ptr);
}
