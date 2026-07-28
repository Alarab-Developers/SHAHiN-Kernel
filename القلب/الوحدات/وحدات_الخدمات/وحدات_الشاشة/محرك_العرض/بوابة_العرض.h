#ifndef DISPLAY_API_H
#define DISPLAY_API_H

#include <stdint.h>

typedef struct
{
    void (*init)(void);

    void (*render)(void);

    void (*set_background)(uint32_t color);

} display_api_t;

extern display_api_t display_api;

#endif
