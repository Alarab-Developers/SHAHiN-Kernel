#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "القلب/المكتبات/المكتبات.h"
#include "القلب/معلومات_المقلع.h"

void مسح_الشاشة(boot_info_t *boot, uint32_t color);

void fb_init_from_boot(boot_info_t *boot);

void fb_putpixel(int x, int y, uint32_t color);
void draw_char(
    int x,
    int y,
    char c,
    uint32_t color
);

void draw_string(
    int x,
    int y,
    const char* str,
    uint32_t color
);

void fb_clear(uint32_t color);

void fb_swap_buffers();

uint32_t* fb_get_back_buffer();
uint32_t* fb_get_front_buffer();

uint32_t fb_get_width();
uint32_t fb_get_height();
uint32_t fb_get_pitch();
uint32_t fb_get_buffer_size();

void draw_rect(
    int x,
    int y,
    int width,
    int height,
    uint32_t color
);

void fill_rect(
    int x,
    int y,
    int width,
    int height,
    uint32_t color
);

void draw_char(
    int x,
    int y,
    char c,
    uint32_t color
);

void draw_string(
    int x,
    int y,
    const char *text,
    uint32_t color
);

// محرك_الفيديو.h - إضافة
void draw_circle(int cx, int cy, int radius, uint32_t color);
void fill_circle(int cx, int cy, int radius, uint32_t color);

#endif
