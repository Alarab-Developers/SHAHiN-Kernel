#include "القلب/الوحدات/وحدات_الخدمات/وحدات_الشاشة/محرك_الفيديو/بوابة_الفيديو.h"
#include "القلب/الوحدات/وحدات_الخدمات/وحدات_الشاشة/محرك_الفيديو/محرك_الفيديو.h"

static boot_info_t *بيانات_الإقلاع = 0;

void تهيئة_الفيديو(
    boot_info_t *boot
)
{
    بيانات_الإقلاع = boot;

    fb_init_from_boot(boot);
}

void مسح_الشاشة_باللون(uint32_t color)
{
    if (!بيانات_الإقلاع)
        return;

    /*
     * بعد تفعيل المخزن المزدوج، يجب أن يمر أي مسح للشاشة عبر
     * fb_clear (يعمل على المخزن الخلفي) ثم fb_swap_buffers
     * (ينسخه دفعة واحدة إلى الشاشة الحقيقية). الاستدعاء المباشر
     * القديم لدالة مسح_الشاشة كان يكتب على ذاكرة الشاشة الحقيقية
     * مباشرة، وأي fb_swap_buffers() تالٍ كان سيمحو هذا التغيير
     * فورًا لأن المخزن الخلفي لا يعلم به — وهذا أحد أسباب الوميض.
     */
    fb_clear(color);
    fb_swap_buffers();
}

/* ========================================================= */
/* WRAPPERS                                                    */
/* ========================================================= */

static void api_putpixel(
    int x,
    int y,
    uint32_t color
)
{
    fb_putpixel(x, y, color);
}

static void api_clear(
    uint32_t color
)
{
    fb_clear(color);
}

static void api_swap()
{
    fb_swap_buffers();
}

static uint32_t api_width()
{
    return fb_get_width();
}

static uint32_t api_height()
{
    return fb_get_height();
}

static uint32_t api_pitch()
{
    return fb_get_pitch();
}

static uint32_t* api_get_back_buffer()
{
    return fb_get_back_buffer();
}





static void api_draw_rect(
    int x,
    int y,
    int width,
    int height,
    uint32_t color
)
{
    draw_rect(
        x,
        y,
        width,
        height,
        color
    );
}

static void api_fill_rect(
    int x,
    int y,
    int width,
    int height,
    uint32_t color
)
{
    fill_rect(
        x,
        y,
        width,
        height,
        color
    );
}


static void api_fill_circle(int cx, int cy, int radius, uint32_t color)
{
    fill_circle(cx, cy, radius, color);
}


/* ========================================================= */
/* API TABLE                                                   */
/* ========================================================= */

video_api_t video_api = {

    .putpixel        = api_putpixel,
    .clear           = api_clear,
    .swap            = api_swap,

    .width           = api_width,
    .height          = api_height,
    .pitch           = api_pitch,
    
    .draw_rect = api_draw_rect,
    .fill_rect = api_fill_rect,
    .fill_circle = api_fill_circle,

    .get_back_buffer = api_get_back_buffer
};
