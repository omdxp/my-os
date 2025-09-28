#include "myos.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

int main(int argc, char **argv)
{
	print(argc == 1 ? "MYOS v1.0.0\n" : "Hello, World from user land!\n");
	return 0;
}
