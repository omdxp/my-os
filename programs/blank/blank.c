#include "myos.h"
#include "stdlib.h"

int main(int argc, char **argv)
{
	print("Hello from user program!\n");

	void *ptr = malloc(512);
	free(ptr);

	while (42)
	{
		if (getkey() != 0)
		{
			print("key pressed!\n");
		}
	}

	return 0;
}
