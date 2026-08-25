#ifndef STRING_H
#define STRING_H

#include "انواع.h"


int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);

char* strcpy(char* dest, const char* src);
char* strcat(char* dest, const char* src);

size_t strlen(const char* s);
int utf8_strlen(const char* s);

void* memset(void* dest, int val, size_t size);
void* memcpy(void* dest, const void* src, size_t size);
int memcmp(const void* a, const void* b, size_t size);

void hex_to_string(uint32_t value, char* str);

#endif
