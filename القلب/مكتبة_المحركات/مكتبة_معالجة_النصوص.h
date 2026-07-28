#ifndef STRING_H
#define STRING_H
#include <stdint.h>

void* memset(void* dest, int val, unsigned long size);
int strcmp(const char* a, const char* b);
void strcpy(char* dest, const char* src);
int strlen(const char* s);
int memcmp(const void* a, const void* b, unsigned long size);

void hex_to_string(uint32_t value, char *str);

int strncmp(const char* a, const char* b, unsigned long n);
char* strcat(char* dest, const char* src);

#endif
