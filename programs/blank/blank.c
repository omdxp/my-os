#include "myos.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

int main(int argc, char **argv)
{
	char words[] = "hello from program";
	const char *token = strtok(words, " ");
	while (token)
	{
		printf("%s\n", token);
		token = strtok(NULL, " ");
	}

	while (42)
	{
	}

	return 0;
}
