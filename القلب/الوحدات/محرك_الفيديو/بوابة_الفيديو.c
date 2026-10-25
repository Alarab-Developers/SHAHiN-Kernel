#include "القلب/الوحدات/محرك_الفيديو/بوابة_الفيديو.h"
#include "القلب/الوحدات/محرك_الفيديو/محرك_الفيديو.h"

static boot_info_t *boot_data = 0;

void video_init(boot_info_t *boot)
{
    boot_data = boot;
    fb_init_from_boot(boot);
}

void video_clear_screen(uint32_t color)
{
    if (!boot_data)
        return;

    fb_clear(color);
    fb_swap_buffers();
}

/* ========================================================= */
/* WRAPPERS                                                  */
/* ========================================================= */

static void api_init(void *fb_info)
{
    video_init((boot_info_t *)fb_info);
}

static void api_clear(uint32_t color)
{
    fb_clear(color);
}

static void api_swap(void)
{
    fb_swap_buffers();
}

static uint32_t api_width(void)
{
    return fb_get_width();
}

static uint32_t api_height(void)
{
    return fb_get_height();
}

static uint32_t api_pitch(void)
{
    return fb_get_pitch();
}

static uint32_t *api_get_back_buffer(void)
{
    return fb_get_back_buffer();
}

/* ========================================================= */
/* API TABLE                                                 */
/* ========================================================= */

video_api_t video_api = {

    .init            = api_init,

    .clear           = api_clear,
    .swap            = api_swap,

    .width           = api_width,
    .height          = api_height,
    .pitch           = api_pitch,

    .get_back_buffer = api_get_back_buffer
};
