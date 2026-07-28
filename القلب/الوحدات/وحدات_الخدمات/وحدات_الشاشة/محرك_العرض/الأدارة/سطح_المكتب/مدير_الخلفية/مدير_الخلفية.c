#include "مدير_الخلفية.h"

static desktop_t desktop;

void background_init(void)
{
    desktop.background_color = 0x202020;
}

void background_set_color(uint32_t color)
{
    desktop.background_color = color;
}

uint32_t background_get_color(void)
{
    return desktop.background_color;
}
