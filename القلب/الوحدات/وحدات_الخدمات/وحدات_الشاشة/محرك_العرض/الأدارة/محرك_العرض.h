#ifndef DISPLAY_SERVER_H
#define DISPLAY_SERVER_H

#include <stdint.h>

void draw_text(int x, int y, const char *text, uint32_t color);
void display_init(void);
void display_set_background(uint32_t color);
void display_render(void);
void display_update(void);

#endif
