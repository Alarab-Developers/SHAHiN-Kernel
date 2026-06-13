#ifndef STRING_H
#define STRING_H

void* memset(void* dest, int val, unsigned long size);
int strcmp(const char* a, const char* b);
void strcpy(char* dest, const char* src);
int strlen(const char* s);
int memcmp(
    const void* a,
    const void* b,
    unsigned long size
);

#endif
