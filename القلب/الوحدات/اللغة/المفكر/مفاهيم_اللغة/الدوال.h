#ifndef الدوال_H
#define الدوال_H

#include "القلب/المكتبات/المكتبات.h"

typedef struct
{
    char name[64];
    char **parameters;          // مصفوفة ديناميكية من أسماء المعاملات
    int parameter_count;
    char **lines;               // مصفوفة ديناميكية من أسطر الدالة
    int line_count;
} arp_function_t;

void functions_init(void);

int function_register(
    const char *name,
    const char **parameters,
    int parameter_count,
    const char **lines,
    int line_count
);

arp_function_t *function_find(
    const char *name
);

#endif
