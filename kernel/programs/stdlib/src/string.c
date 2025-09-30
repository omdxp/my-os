#include "string.h"

char tolower(char c)
{
	if (c >= 65 && c <= 90)
	{
		c += 32;
	}
	return c;
}

int strlen(const char *ptr)
{
	int i = 0;
	while (*ptr != 0)
	{
		i++;
		ptr += 1;
	}

	return i;
}

int strnlen(const char *ptr, int max)
{
	int i = 0;
	for (i = 0; i < max; i++)
	{
		if (ptr[i] == 0)
		{
			break;
		}
	}

	return i;
}

int strnlen_terminator(const char *str, int max, char terminator)
{
	int i = 0;
	for (i = 0; i < max; i++)
	{
		if (str[i] == '\0' || str[i] == terminator)
			break;
	}
	return i;
}

int istrncmp(const char *s1, const char *s2, int n)
{
	unsigned char u1, u2;
	while (n-- > 0)
	{
		u1 = (unsigned char)*s1++;
		u2 = (unsigned char)*s2++;
		if (u1 != u2 && tolower(u1) != tolower(u2))
			return u1 - u2;
		if (u1 == '\0')
			return 0;
	}
	return 0;
}

int strncmp(const char *str1, const char *str2, int n)
{
	unsigned char u1, u2;
	while (n-- > 0)
	{
		u1 = (unsigned char)*str1++;
		u2 = (unsigned char)*str2++;
		if (u1 != u2)
			return u1 - u2;
		if (u1 == '\0')
			return 0;
	}
	return 0;
}

char *strcpy(char *dest, const char *src)
{
	char *res = dest;
	while (*src != 0)
	{
		*dest = *src;
		src += 1;
		dest += 1;
	}
	*dest = 0x00;
	return res;
}

char *strncpy(char *dest, const char *src, int count)
{
	int i = 0;
	for (i = 0; i < count - 1; i++)
	{
		if (src[i] == 0x00)
			break;
		dest[i] = src[i];
	}

	dest[i] = 0x00;
	return dest;
}

bool isdigit(char c)
{
	return c >= 48 && c <= 57;
}

int tonumericdigit(char c)
{
	return c - 48;
}

char *sp = 0;
char *strtok(char *str, const char *delimiters)
{
	int i = 0;
	int len = strlen(delimiters);
	if (!str && !sp)
	{
		return 0;
	}

	if (str && !sp)
	{
		sp = str;
	}

	char *p_start = sp;
	while (42)
	{
		for (i = 0; i < len; i++)
		{
			if (*p_start == delimiters[i])
			{
				p_start++;
				break;
			}
		}

		if (i == len)
		{
			sp = p_start;
			break;
		}
	}

	if (*sp == '\0')
	{
		sp = 0;
		return sp;
	}

	// find end of substring
	while (*sp != '\0')
	{
		for (i = 0; i < len; i++)
		{
			if (*sp == delimiters[i])
			{
				*sp = '\0';
				break;
			}
		}

		sp++;
		if (i < len)
		{
			break;
		}
	}

	return p_start;
}

char *strstr(const char *haystack, const char *needle)
{
	if (*needle == 0)
	{
		return (char *)haystack;
	}

	char *p1 = (char *)haystack;
	while (*p1 != 0)
	{
		if (*p1 == *needle)
		{
			char *p1Begin = p1;
			char *p2 = (char *)needle;
			while (*p1 != 0 && *p2 != 0 && *p1 == *p2)
			{
				p1++;
				p2++;
			}

			if (*p2 == 0)
			{
				return p1Begin;
			}
		}
		p1++;
	}

	return 0;
}

char *strcat(char *dest, const char *src)
{
	char *ptr = dest + strlen(dest);
	while (*src != 0)
	{
		*ptr = *src;
		ptr++;
		src++;
	}
	*ptr = 0;
	return dest;
}

char *strncat(char *dest, const char *src, int count)
{
	char *ptr = dest + strlen(dest);
	int i = 0;
	for (i = 0; i < count; i++)
	{
		if (src[i] == 0)
			break;
		ptr[i] = src[i];
	}
	ptr[i] = 0;
	return dest;
}

char *strrchr(const char *s, int c)
{
	char *last = 0;
	for (;;)
	{
		if (*s == c)
			last = (char *)s;
		if (*s == '\0')
			return last;
		s++;
	}
}
