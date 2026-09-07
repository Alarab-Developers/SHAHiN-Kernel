#ifndef ARP_CONTEXT_H
#define ARP_CONTEXT_H

#include "القلب/الوحدات/اللغة/المفكر/مفاهيم_اللغة/الدوال.h"
#include "القلب/الوحدات/اللغة/المفكر/مفاهيم_اللغة/المتغيرات.h"

typedef struct
{
    arp_function_t *functions;     // مصفوفة ديناميكية
    int function_count;
    int functions_capacity;

    variable_t variables[MAX_VARIABLES];  // يمكن جعلها ديناميكية لاحقاً

} arp_context_t;

arp_context_t* arp_get_current_context(void);

#endif
