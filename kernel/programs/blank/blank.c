#include "myos.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "file.h"

int main(int argc, char **argv)
{
	int fd = fopen("@:/blank.elf", "r");
	if (fd > 0)
	{
		printf("Successfully opened file blank.elf with fd %d\n", fd);
	}
	else
	{
		printf("Failed to open file blank.elf\n");
	}

	while (1)
		;

	return 0;
}
