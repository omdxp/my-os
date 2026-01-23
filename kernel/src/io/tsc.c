#include "tsc.h"
#include "cpuid.h"

uint64_t tsc_freq_val = 0;

TIME_TSC tsc_frequency(void)
{
	if (tsc_freq_val != 0)
	{
		return tsc_freq_val;
	}

	uint32_t eax, ebx, ecx, edx;
	uint64_t tsc_freq = 0;

	// 0x15: TSC and Nominal Core Crystal Clock Information
	cpuid(0x15, 0, &eax, &ebx, &ecx, &edx);
	if (eax != 0 && ebx != 0 && ecx != 0)
	{
		// TSC Frequency = (EBX / EAX) * ECX
		tsc_freq = ((uint64_t)ebx * (uint64_t)ecx) / (uint64_t)eax;
	}

	// 0x16: Processor Frequency Information (as fallback)
	if (tsc_freq == 0)
	{
		cpuid(0x16, 0, &eax, &ebx, &ecx, &edx);
		if (eax != 0)
		{
			// EAX: Base Frequency in MHz
			tsc_freq = (uint64_t)eax * 1000000ULL;
		}
	}

	// get tsc frequency value in Hz
	tsc_freq *= 1000000;
	tsc_freq_val = tsc_freq;

	return (TIME_TSC)tsc_freq_val;
}

TIME_TSC read_tsc(void)
{
	uint32_t low, high;
	__asm__ volatile("lfence; rdtsc"
					 : "=a"(low), "=d"(high)::"memory");
	return ((TIME_TSC)high << 32) | (TIME_TSC)low;
}

TIME_MILLISECONDS tsc_milliseconds(void)
{
	TIME_MICROSECONDS microseconds = tsc_microseconds();
	return (TIME_MILLISECONDS)(microseconds / 1000);
}

TIME_SECONDS tsc_seconds(void)
{
	TIME_MILLISECONDS milliseconds = tsc_milliseconds();
	return (TIME_SECONDS)(milliseconds / 1000);
}

void udelay(TIME_MICROSECONDS microseconds)
{
	TIME_TSC tsc_freq = tsc_frequency();
	TIME_TSC start_tsc = read_tsc();
	TIME_TSC cycles_to_wait = (tsc_freq / 1000000) * microseconds;
	while ((read_tsc() - start_tsc) < cycles_to_wait)
	{
		__asm__ volatile("pause");
	}
}