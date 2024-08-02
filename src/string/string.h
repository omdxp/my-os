#pragma once

#include <stdbool.h>

int strlen(const char *ptr);
bool isdigit(char c);
int tonumericdigit(char c);
int strnlen(const char *ptr, int max);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, int count);
int strncmp(const char *str1, const char *str2, int n);
int istrncmp(const char *s1, const char *s2, int n);
int strnlen_terminator(const char *str, int max, char terminator);
char tolower(char c);
char *strstr(const char *haystack, const char *needle);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, int count);
char *strrchr(const char *s, int c);
