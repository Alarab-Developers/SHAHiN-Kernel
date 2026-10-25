#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "القلب/المكتبات/المكتبات.h"
#include "القلب/معلومات_المقلع.h"

/* =========================================================
 * دورة الحياة والتحكم
 * ========================================================= */

void fb_init_from_boot(boot_info_t *boot);

void fb_clear(uint32_t color);
void fb_swap_buffers(void);

/* =========================================================
 * معلومات الشاشة والوصول للبافر
 * ========================================================= */

uint32_t *fb_get_back_buffer(void);
uint32_t *fb_get_front_buffer(void);

uint32_t fb_get_width(void);
uint32_t fb_get_height(void);
uint32_t fb_get_pitch(void);
uint32_t fb_get_buffer_size(void);

#endif
