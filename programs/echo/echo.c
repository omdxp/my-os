#include "myos.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++)
	{
		print(argv[i]);
		print(" ");
	}
	print("\n");
	return 0;
}
