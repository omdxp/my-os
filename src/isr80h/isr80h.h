#pragma once

enum system_commands
{
	SYSTEM_COMMAND0_SUM,
	SYSTEM_COMMAND1_PRINT
};

void isr80h_register_commands();
