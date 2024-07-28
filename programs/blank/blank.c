#include "myos.h"
#include "stdlib.h"
#include "stdio.h"

int main(int argc, char **argv)
{
	print("Hello from user program!\n");
	print(itoa(2024));

	void *ptr = malloc(512);
	free(ptr);

	while (42)
	{
		int c = getkey();
		if (c != 0)
		{
			putchar(c);
		}
	}

	return 0;
}
