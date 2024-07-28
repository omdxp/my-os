#include "stdio.h"
#include "myos.h"

int putchar(int c)
{
	myos_putchar((char)c);
	return 0;
}
