#ifndef VIDEO_API_H
#define VIDEO_API_H

#include "القلب/المكتبات/المكتبات.h"
#include "القلب/معلومات_المقلع.h"

void video_init(boot_info_t *boot);

void video_clear_screen(uint32_t color);

typedef struct {

    /* framebuffer */

    void (*init)(void *fb_info);

    /* الخلفية: مسح كامل بلون واحد */

    void (*clear)(uint32_t color);

    /* تبديل البافر الخلفي إلى الأمامي */

    void (*swap)(void);

    /* screen info */

    uint32_t (*width)(void);
    uint32_t (*height)(void);
    uint32_t (*pitch)(void);

    /* framebuffer access */

    uint32_t *(*get_back_buffer)(void);

} video_api_t;

extern video_api_t video_api;

#endif
