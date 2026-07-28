#include "محرك_العرض.h"
#include "القلب/الوحدات/مدير_الوحدات.h"
#include "سطح_المكتب/مدير_الخلفية/مدير_الخلفية.h"
#include "سطح_المكتب/مدير_المؤشر/مدير_المؤشر.h"
#include "سطح_المكتب/مدير_النوافذ/مدير_النوافذ.h"
#include "سطح_المكتب/مدير_النوافذ/التصدير/خط_تجريبي.h"
#include "القلب/الوحدات/وحدات_الخدمات/وحدات_الشاشة/محرك_الفيديو/بوابة_الفيديو.h"
#include "القلب/اختبار/لوحة_المفاتيح/لوحة_المفاتيح.h"
#include "القلب/اختبار/طرفية/طرفية.h"

// دوال الرسم المطابقة لـ window_drawing_t
static void draw_fill_rect(int x, int y, int width, int height, uint32_t color)
{
    video_api.fill_rect(x, y, width, height, color);
}

static void draw_fill_circle(int cx, int cy, int radius, uint32_t color)
{
    video_api.fill_circle(cx, cy, radius, color);
}


static int draw_get_screen_width(void)
{
    return video_api.width();
}

static int draw_get_screen_height(void)
{
    return video_api.height();
}


static void draw_char(
    int x,
    int y,
    char c,
    uint32_t color
)
{
    const uint8_t *glyph = fonts8x8[(uint8_t)c];


    for(int row = 0; row < 8; row++)
    {
        uint8_t bits = glyph[row];

        for(int col = 0; col < 8; col++)
        {
            if(bits & (1 << (7 - col)))
            {
                video_api.putpixel(
                    x + col,
                    y + row,
                    color
                );
            }
        }
    }
}


void draw_text(
    int x,
    int y,
    const char *text,
    uint32_t color
)
{
    while(*text)
    {
        draw_char(
            x,
            y,
            *text,
            color
        );

        x += 8;
        text++;
    }
}





void display_init(void)
{
    background_init();
    cursor_init();

    // تهيئة مدير النوافذ
    window_manager_init();




    // تمرير دوال الرسم إلى مدير النوافذ
    window_drawing_t drawing = {
        .fill_rect = draw_fill_rect,
        .fill_circle = draw_fill_circle,
        .draw_text = draw_text,
        .get_screen_width = draw_get_screen_width,
        .get_screen_height = draw_get_screen_height
    };


    
    window_set_drawing_functions(&drawing);

    window_create(
        100,
        100,
        400,
        300,
        0x00000000,
        "Terminal"
    );
    
    terminal_init();
}

void display_set_background(uint32_t color)
{
    background_set_color(color);
}

void display_update(void)
{
    window_manager_update(
        mouse_x,
        mouse_y,
        mouse_left
    );

    window_manager_update(
            mouse_x,
            mouse_y,
            mouse_left
    );

    terminal_update();



}

void display_render(void)
{
    const cursor_t *cursor = cursor_get();

    // مسح الخلفية
    video_api.clear(background_get_color());

    // تحديث النوافذ
    display_update();

    // رسم جميع النوافذ
    window_draw_all();
    
    terminal_render();

    // رسم المؤشر
    video_api.fill_rect(
        mouse_x,
        mouse_y,
        cursor->width,
        cursor->height,
        cursor->color
    );

    video_api.swap();
}

