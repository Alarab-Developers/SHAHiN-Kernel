#ifndef CURSOR_MANAGER_H
#define CURSOR_MANAGER_H

#include <stdint.h>

typedef struct
{
    int width;
    int height;
    uint32_t color;
} cursor_t;



void cursor_init(void);

const cursor_t* cursor_get(void);

#endif
