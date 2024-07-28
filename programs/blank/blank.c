#include "myos.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

int main(int argc, char **argv)
{
	struct process_arguments arguments;
	myos_process_get_arguments(&arguments);

	printf("\n%i %s\n", arguments.argc, arguments.argv[0]);

	while (42)
	{
	}

	return 0;
}
