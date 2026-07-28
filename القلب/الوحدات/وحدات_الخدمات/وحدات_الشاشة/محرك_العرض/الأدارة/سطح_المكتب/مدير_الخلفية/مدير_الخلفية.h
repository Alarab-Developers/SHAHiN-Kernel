#ifndef BACKGROUND_MANAGER_H
#define BACKGROUND_MANAGER_H

#include <stdint.h>

typedef struct
{
    uint32_t background_color;

} desktop_t;

void background_init(void);

void background_set_color(uint32_t color);

uint32_t background_get_color(void);

#endif
