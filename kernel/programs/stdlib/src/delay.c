#include "delay.h"
#include "myos.h"

void udelay(uint64_t microseconds)
{
	myos_udelay(microseconds);
}

void usleep(uint64_t miliseconds)
{
	udelay(miliseconds * 1000);
}