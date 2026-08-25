#include "المكتبات.h"

int strcmp(const char* a, const char* b)
{
    while (*a && (*a == *b))
    {
        a++;
        b++;
    }

    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char* a, const char* b, size_t n)
{
    while (n--)
    {
        if (*a != *b)
            return (unsigned char)*a - (unsigned char)*b;

        if (*a == '\0')
            return 0;

        a++;
        b++;
    }

    return 0;
}

char* strcpy(char* dest, const char* src)
{
    char* ret = dest;

    while ((*dest++ = *src++))
        ;

    return ret;
}

char* strcat(char* dest, const char* src)
{
    char* ret = dest;

    while (*dest)
        dest++;

    while ((*dest++ = *src++))
        ;

    return ret;
}

size_t strlen(const char* s)
{
    const char* p = s;

    while (*p)
        p++;

    return (size_t)(p - s);
}

int utf8_strlen(const char* s)
{
    int count = 0;

    while (*s)
    {
        if (((unsigned char)*s & 0xC0) != 0x80)
            count++;

        s++;
    }

    return count;
}

void* memset(void* dest, int val, size_t size)
{
    unsigned char* d = (unsigned char*)dest;

    while (size--)
        *d++ = (unsigned char)val;

    return dest;
}

void* memcpy(void* dest, const void* src, size_t size)
{
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;

    while (size--)
        *d++ = *s++;

    return dest;
}

int memcmp(const void* a, const void* b, size_t size)
{
    const unsigned char* p1 = (const unsigned char*)a;
    const unsigned char* p2 = (const unsigned char*)b;

    while (size--)
    {
        if (*p1 != *p2)
            return *p1 - *p2;

        p1++;
        p2++;
    }

    return 0;
}

void hex_to_string(uint32_t value, char* str)
{
    static const char hex[] = "0123456789ABCDEF";

    for (int i = 7; i >= 0; i--)
    {
        str[i] = hex[value & 0xF];
        value >>= 4;
    }

    str[8] = '\0';
}
