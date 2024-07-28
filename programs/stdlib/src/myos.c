#include "myos.h"

int myos_getkeyblock()
{
	int val = myos_getkey();
	do
	{
		val = myos_getkey();
	} while (val == 0);

	return val;
}

void myos_terminal_readline(char *out, int max, bool output_while_typing)
{
	int i = 0;
	for (i = 0; i < max - 1; i++)
	{
		char key = myos_getkeyblock();
		// carriage returns (next line)
		if (key == 13)
		{
			break;
		}

		if (output_while_typing)
		{
			myos_putchar(key);
		}

		// backspace
		if (key == 0x08 && i >= 1)
		{
			out[i - 1] = 0x00;
			i -= 2; // because i will be +1 when continue
			continue;
		}

		out[i] = key;
	}

	// add null terminator
	out[i] = 0x00;
}
