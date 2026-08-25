#ifndef VIDEO_API_H
#define VIDEO_API_H

#include "القلب/المكتبات/المكتبات.h"
#include "القلب/معلومات_المقلع.h"

void تهيئة_الفيديو(boot_info_t *boot);

void مسح_الشاشة_باللون(uint32_t color);

typedef struct {

    /* framebuffer */

    void (*init)(
        void* fb_info
    );

    /* drawing */

    void (*putpixel)(
        int x,
        int y,
        uint32_t color
    );

    void (*clear)(
        uint32_t color
    );

    void (*swap)();

    /* screen info */

    uint32_t (*width)();
    uint32_t (*height)();
    uint32_t (*pitch)();

    /* framebuffer access */

    uint32_t* (*get_back_buffer)();


    void (*draw_rect)(
        int x,
        int y,
        int width,
        int height,
        uint32_t color
    );

    void (*fill_rect)(
        int x,
        int y,
        int width,
        int height,
        uint32_t color
    );
    

    void (*fill_circle)(int cx, int cy, int radius, uint32_t color);

} video_api_t;

extern video_api_t video_api;

#endif
