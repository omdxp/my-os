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
		char buffer[128] = {0};
		int bytes_read = fread(buffer, 1, sizeof(buffer) - 1, fd);
		if (bytes_read > 0)
		{
			buffer[bytes_read] = '\0'; // null-terminate the string
			printf("Read %d bytes from blank.elf:\n%s\n", bytes_read, buffer);
		}

		int seek_result = fseek(fd, 0, 0); // SEEK_SET
		if (seek_result == 0)
		{
			printf("Successfully seeked to the beginning of blank.elf\n");
		}
	}
	else
	{
		printf("Failed to open file blank.elf\n");
	}

	fclose(fd);
	printf("Closed file blank.elf\n");

	while (1)
		;

	return 0;
}
