#pragma once

#include <stdint.h>

typedef uint64_t TIME_TSC;
typedef uint64_t TIME_MICROSECONDS;
typedef uint64_t TIME_MILLISECONDS;
typedef uint64_t TIME_SECONDS;

TIME_TSC tsc_frequency(void);
void udelay(TIME_MICROSECONDS microseconds);
TIME_SECONDS tsc_seconds(void);
TIME_MILLISECONDS tsc_milliseconds(void);
TIME_TSC read_tsc(void);
TIME_MICROSECONDS tsc_microseconds(void);
