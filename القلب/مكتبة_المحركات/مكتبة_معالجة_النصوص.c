#include "مكتبة_معالجة_النصوص.h"

int strcmp(const char* a, const char* b)
{
    int i = 0;

    while (a[i] && b[i]) {

        if (a[i] != b[i])
            return a[i] - b[i];

        i++;
    }

    return a[i] - b[i];
}

void strcpy(char* dest, const char* src)
{
    int i = 0;

    while (src[i]) {
        dest[i] = src[i];
        i++;
    }

    dest[i] = 0;
}

int strlen(const char* s)
{
    int i = 0;

    while (s[i])
        i++;

    return i;
}

void* memset(void* dest, int val, unsigned long size) {
    unsigned char* d = dest;
    for (unsigned long i = 0; i < size; i++) {
        d[i] = (unsigned char)val;
    }
    return dest;
}


void* memcpy(void* dest, const void* src, unsigned long size) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;

    for (unsigned long i = 0; i < size; i++) {
        d[i] = s[i];
    }

    return dest;
}


int memcmp(
    const void* a,
    const void* b,
    unsigned long size
)
{
    const unsigned char* p1 = a;
    const unsigned char* p2 = b;

    for (unsigned long i = 0; i < size; i++) {

        if (p1[i] != p2[i])
            return p1[i] - p2[i];
    }

    return 0;
}



int utf8_strlen(const char* s)
{
    int count = 0;

    while (*s)
    {
        unsigned char c = (unsigned char)*s;

        /* بداية محرف UTF-8 */

        if ((c & 0xC0) != 0x80)
            count++;

        s++;
    }

    return count;
}
