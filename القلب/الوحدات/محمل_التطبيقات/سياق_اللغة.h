#ifndef ARP_CONTEXT_H
#define ARP_CONTEXT_H

#include "القلب/الوحدات/اللغة/مفاهيم_اللغة/الدوال.h"
#include "القلب/الوحدات/اللغة/مفاهيم_اللغة/المتغيرات.h"

#define ARP_MAX_IMPORTED_LIBS 256

typedef struct
{
    arp_function_t *functions;
    int function_count;
    int functions_capacity;

    variable_t *variables;
    int variable_count;
    int variables_capacity;

    void *active_vm;

    int current_window_id;

    char *imported_libs[ARP_MAX_IMPORTED_LIBS];
    int   imported_count;
} arp_context_t;

arp_context_t *arp_get_current_context(void);

/* تعيين سياق محلي صريح */
void arp_set_current_context(arp_context_t *ctx);

/* ★ جديد: تثبيت سياق مؤقت بأولوية عالية */
void arp_set_override_context(arp_context_t *ctx);

#endif
