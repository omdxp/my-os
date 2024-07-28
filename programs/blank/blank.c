#include "myos.h"

int main(int argc, char **argv)
{
	print("Hello from user program!\n");
	while (42)
	{
		if (getkey() != 0)
		{
			print("key pressed!\n");
		}
	}

	return 0;
}
