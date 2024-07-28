#include "myos.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

int main(int argc, char **argv)
{
	for (int i = 0; i < argc; i++)
	{
		printf("%s\n", argv[i]);
	}

	while (42)
	{
	}

	return 0;
}
