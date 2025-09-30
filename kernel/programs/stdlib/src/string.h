#pragma once

#include <stdbool.h>

char tolower(char c);
int strlen(const char *ptr);
int strnlen(const char *ptr, int max);
int strnlen_terminator(const char *str, int max, char terminator);
int istrncmp(const char *s1, const char *s2, int n);
int strncmp(const char *str1, const char *str2, int n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, int count);
bool isdigit(char c);
int tonumericdigit(char c);
char *strtok(char *str, const char *delimiters);
char *strstr(const char *haystack, const char *needle);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, int count);
char *strrchr(const char *s, int c);
