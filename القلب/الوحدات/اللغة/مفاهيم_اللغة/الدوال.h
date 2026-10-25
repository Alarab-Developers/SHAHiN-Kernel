#ifndef الدوال_H
#define الدوال_H

#include "القلب/المكتبات/المكتبات.h"

typedef struct
{
    char *name;                /* ديناميكي */
    char **parameters;
    int    parameter_count;
    char **lines;
    int    line_count;

    /* JIT Cache: كود x86_64 مُترجَم مسبقًا لهذه الدالة. */
    void   *jit_code;
    size_t  jit_code_size;
} arp_function_t;

void functions_init(void);

int function_register(
    const char *name,
    const char **parameters,
    int parameter_count,
    const char **lines,
    int line_count
);

arp_function_t *function_find(const char *name);

void function_free(arp_function_t *function);

/* تسجيل دالة مع كود JIT جاهز مسبقًا */
int function_register_aot(
    const char *name,
    const char **parameters,
    int parameter_count,
    void *jit_code,
    size_t jit_code_size
);

#endif
