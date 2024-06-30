#pragma once

#include <stdbool.h>

int strlen(const char *ptr);
bool isdigit(char c);
int tonumericdigit(char c);
int strnlen(const char *ptr, int max);
char *strcpy(char *dest, const char *src);
