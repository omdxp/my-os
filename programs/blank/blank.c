#include "myos.h"
#include "stdlib.h"
#include "stdio.h"

int main(int argc, char **argv)
{
	print("Hello from user program!\n");
	print(itoa(2024));

	void *ptr = malloc(512);
	free(ptr);

	printf("\n%i bytes was allocated for %s\n", 512, "ptr");

	char buf[1024];
	myos_terminal_readline(buf, sizeof(buf), true);
	print(buf);

	while (42)
	{
	}

	return 0;
}
