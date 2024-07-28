#include "shell.h"
#include "stdio.h"
#include "stdlib.h"
#include "myos.h"

int main(int argc, char **argv)
{
	print("MYOS v1.0.0\n");
	while (42)
	{
		print("$ ");
		char buf[1024];
		myos_terminal_readline(buf, sizeof(buf), true);
		myos_process_load_start(buf);
		print("\n");
	}

	return 0;
}
