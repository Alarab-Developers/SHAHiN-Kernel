#ifndef المفسر_H
#define المفسر_H

#include "القلب/المكتبات/المكتبات.h"
#include "شجرة.h"

int arp_invoke_function_typed(
    const char *name,
    const arp_value_t *arguments,
    int argument_count,
    arp_value_t *result
);

#endif
