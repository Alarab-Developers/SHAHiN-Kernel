#ifndef DEVELOPMENT_SCREEN_H
#define DEVELOPMENT_SCREEN_H

#include "القلب/المكتبات/المكتبات.h"

void development_screen_init(void);

void development_screen_print(
    const char *text
);

void development_screen_println(
    const char *text
);

void development_screen_print_hex64(
    uint64_t value
);

void development_screen_update(void);

#endif
