#include "محرك_الفيديو.h"
#include "الخط.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"   // لإستخدام memory_api.alloc/free

/* إزالة التعريف الثابت والمصفوفة الثابتة السابقة */

static uint32_t *fb_back_dynamic = NULL;   // المخزن الخلفي المخصص ديناميكياً
static uint32_t  fb_back_fallback[1024 * 768]; // احتياطي (حوالي 3 م.ب) في حال فشل التخصيص

static uint32_t *fb         = 0;   // pointer to active back buffer
static uint32_t *fb_front   = 0;   // pointer to real framebuffer (front)
static uint32_t  fb_width   = 0;
static uint32_t  fb_height  = 0;
static uint32_t  fb_pitch   = 0;
static uint32_t  buffer_size = 0;   // عدد البكسلات

void fb_init_from_boot(boot_info_t* boot)
{
    if (!boot)              return;
    if (!boot->FrameBuffer) return;

    fb_front    = (uint32_t*)boot->FrameBuffer;
    fb_width    = boot->Width;
    fb_height   = boot->Height;
    fb_pitch    = boot->PixelsPerScanLine;

    if (!fb_pitch) {
        fb_front = 0;
        return;
    }

    // حساب عدد البكسلات اللازم للمخزن الخلفي
    buffer_size = fb_pitch * fb_height;

    // تحرير المخزن الخلفي القديم إن وجد
    if (fb_back_dynamic) {
        memory_api.free(fb_back_dynamic);
        fb_back_dynamic = NULL;
    }

    // محاولة التخصيص الديناميكي
    fb_back_dynamic = (uint32_t*)memory_api.alloc(buffer_size * sizeof(uint32_t));

    if (fb_back_dynamic) {
        fb = fb_back_dynamic;
    } else {
        // فشل التخصيص → استخدم المخزن الاحتياطي الثابت
        fb = fb_back_fallback;
        // إذا كان الحجم المطلوب أكبر من سعة الاحتياطي، نقتصر على حجم الاحتياطي
        if (buffer_size > (sizeof(fb_back_fallback) / sizeof(uint32_t))) {
            buffer_size = sizeof(fb_back_fallback) / sizeof(uint32_t);
            // ضبط الأبعاد لتتناسب مع الحجم الجديد
            fb_height = buffer_size / fb_pitch;
            if (fb_height > boot->Height)
                fb_height = boot->Height;
            buffer_size = fb_pitch * fb_height;
        }
    }

    // تصفير المخزن الخلفي
    for (uint32_t i = 0; i < buffer_size; i++)
        fb[i] = 0x00000000;

    // عرض الصورة الأولية على الشاشة
    fb_swap_buffers();
}

void fb_putpixel(int x, int y, uint32_t color)
{
    if (!fb)                       return;
    if (x < 0 || y < 0)           return;
    if ((uint32_t)x >= fb_width)   return;
    if ((uint32_t)y >= fb_height)  return;

    fb[y * fb_pitch + x] = color;
}

void fb_clear(uint32_t color)
{
    if (!fb || !buffer_size) return;

    for (uint32_t i = 0; i < buffer_size; i++)
        fb[i] = color;
}

void fb_swap_buffers()
{
    if (!fb_front || !fb || !buffer_size) return;

    uint32_t *dst      = fb_front;
    uint32_t *src      = fb;
    uint64_t  qwords   = buffer_size / 2;   // ننسخ 8 بايت في كل دورة
    uint32_t  remainder = buffer_size & 1;

    __asm__ volatile (
        "cld\n\t"
        "rep movsq"
        : "+D" (dst), "+S" (src), "+c" (qwords)
        :
        : "memory"
    );

    if (remainder)
        dst[0] = src[0];
}

uint32_t* fb_get_back_buffer()  { return fb; }
uint32_t* fb_get_front_buffer() { return fb_front; }
uint32_t  fb_get_width()        { return fb_width; }
uint32_t  fb_get_height()       { return fb_height; }
uint32_t  fb_get_pitch()        { return fb_pitch; }
uint32_t  fb_get_buffer_size()  { return buffer_size; }

void مسح_الشاشة(boot_info_t *boot, uint32_t color)
{
    if (!boot)                    return;
    if (!boot->FrameBuffer)       return;
    if (!boot->PixelsPerScanLine) return;

    volatile uint32_t *fb_local =
        (volatile uint32_t *)boot->FrameBuffer;

    unsigned total = boot->Height * boot->PixelsPerScanLine;

    for (unsigned i = 0; i < total; i++)
        fb_local[i] = color;
}

// باقي دوال الرسم (draw_rect, fill_rect, draw_circle, fill_circle, draw_char, draw_string, fill_triangle)
// تبقى كما هي دون تغيير، لأنها تعتمد على fb_putpixel و fb والذي تم تحديثه.
// سأدرجها هنا للاكتمال، ولكنها لم تتغير.

void draw_rect(int x, int y, int width, int height, uint32_t color)
{
    for (int i = 0; i < width; i++)
    {
        fb_putpixel(x + i, y, color);
        fb_putpixel(x + i, y + height - 1, color);
    }
    for (int i = 0; i < height; i++)
    {
        fb_putpixel(x, y + i, color);
        fb_putpixel(x + width - 1, y + i, color);
    }
}

void fill_rect(int x, int y, int width, int height, uint32_t color)
{
    for (int yy = 0; yy < height; yy++)
        for (int xx = 0; xx < width; xx++)
            fb_putpixel(x + xx, y + yy, color);
}

void draw_circle(int cx, int cy, int radius, uint32_t color)
{
    int x = radius, y = 0, err = 0;
    while (x >= y)
    {
        fb_putpixel(cx + x, cy + y, color);
        fb_putpixel(cx + y, cy + x, color);
        fb_putpixel(cx - y, cy + x, color);
        fb_putpixel(cx - x, cy + y, color);
        fb_putpixel(cx - x, cy - y, color);
        fb_putpixel(cx - y, cy - x, color);
        fb_putpixel(cx + y, cy - x, color);
        fb_putpixel(cx + x, cy - y, color);
        y++;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0)
        {
            x--;
            err += 1 - 2 * x;
        }
    }
}

void fill_circle(int cx, int cy, int radius, uint32_t color)
{
    for (int y = -radius; y <= radius; y++)
    {
        int half_width = 0;
        while (half_width * half_width + y * y < radius * radius)
            half_width++;
        half_width--;
        for (int x = -half_width; x <= half_width; x++)
            fb_putpixel(cx + x, cy + y, color);
    }
}

void draw_char(int x, int y, char c, uint32_t color)
{
    if ((unsigned char)c >= 128) return;
    const uint8_t *glyph = fonts8x8[(uint8_t)c];
    for (int row = 0; row < 8; row++)
    {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++)
            if (bits & (1 << (7 - col)))
                fb_putpixel(x + col, y + row, color);
    }
}

void draw_string(int x, int y, const char *text, uint32_t color)
{
    if (!text) return;
    while (*text)
    {
        if (*text == '\n')
        {
            x = 0;
            y += 8;
            text++;
            continue;
        }
        draw_char(x, y, *text, color);
        x += 8;
        text++;
    }
}

static int64_t triangle_edge(int x1, int y1, int x2, int y2, int x, int y)
{
    return (int64_t)(x - x1) * (y2 - y1) - (int64_t)(y - y1) * (x2 - x1);
}

void fill_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color)
{
    int min_x = x1, max_x = x1;
    if (x2 < min_x) min_x = x2; if (x2 > max_x) max_x = x2;
    if (x3 < min_x) min_x = x3; if (x3 > max_x) max_x = x3;
    int min_y = y1, max_y = y1;
    if (y2 < min_y) min_y = y2; if (y2 > max_y) max_y = y2;
    if (y3 < min_y) min_y = y3; if (y3 > max_y) max_y = y3;

    int64_t area = triangle_edge(x1, y1, x2, y2, x3, y3);
    if (area == 0) return;

    int screen_w = (int)fb_get_width();
    int screen_h = (int)fb_get_height();
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= screen_w) max_x = screen_w - 1;
    if (max_y >= screen_h) max_y = screen_h - 1;
    if (min_x > max_x || min_y > max_y) return;

    for (int y = min_y; y <= max_y; y++)
    {
        for (int x = min_x; x <= max_x; x++)
        {
            int64_t w1 = triangle_edge(x1, y1, x2, y2, x, y);
            int64_t w2 = triangle_edge(x2, y2, x3, y3, x, y);
            int64_t w3 = triangle_edge(x3, y3, x1, y1, x, y);
            if ((w1 >= 0 && w2 >= 0 && w3 >= 0) || (w1 <= 0 && w2 <= 0 && w3 <= 0))
                fb_putpixel(x, y, color);
        }
    }
}
