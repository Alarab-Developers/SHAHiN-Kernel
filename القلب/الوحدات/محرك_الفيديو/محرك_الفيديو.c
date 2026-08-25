#include "محرك_الفيديو.h"
#include "الخط.h"

/*
 * الحد الأقصى لعدد البكسلات التي يستوعبها المخزن الخلفي الثابت.
 * 1920x1080 = 2,073,600 بكسل (~8 ميجابايت). إن كانت دقتك الفعلية
 * أصغر (مثلاً 1280x720) يمكنك تصغير هذا الرقم لتوفير مساحة BSS،
 * أو استبدال fb_back بتخصيص ديناميكي من المخصص الثابت (bump allocator)
 * الذي تستخدمه بالفعل في فك ترميز PNG.
 */
#define FB_MAX_BUFFER_PIXELS (1366u * 768u)

/* المخزن الأمامي: المؤشر الحقيقي إلى ذاكرة الشاشة (GOP framebuffer) */
static uint32_t *fb_front   = 0;

/* المخزن الخلفي: ذاكرة RAM عادية، كل عمليات الرسم تحدث هنا فقط */
static uint32_t  fb_back[FB_MAX_BUFFER_PIXELS];

/*
 * fb تبقى بنفس الاسم الذي تستخدمه كل دوال الرسم القديمة
 * (fb_putpixel, fb_clear, draw_rect ...) لكنها الآن تشير دائمًا
 * إلى المخزن الخلفي وليس إلى الشاشة الحقيقية.
 */
static uint32_t *fb         = 0;

static uint32_t  fb_width   = 0;
static uint32_t  fb_height  = 0;
static uint32_t  fb_pitch   = 0;
static uint32_t  buffer_size = 0;

void fb_init_from_boot(boot_info_t* boot)
{
    if (!boot)              return;
    if (!boot->FrameBuffer) return;

    fb_front    = (uint32_t*)boot->FrameBuffer;
    fb_width    = boot->Width;
    fb_height   = boot->Height;
    fb_pitch    = boot->PixelsPerScanLine;

    if (!fb_pitch)
    {
        /* لا يوجد pitch صالح، لا يمكن حساب أي شيء بأمان */
        fb_front = 0;
        return;
    }


    if ((uint64_t)fb_pitch * fb_height > FB_MAX_BUFFER_PIXELS)
    {
        fb_height = FB_MAX_BUFFER_PIXELS / fb_pitch;
    }


    if (fb_width > fb_pitch)
    {
        fb_width = fb_pitch;
    }

    buffer_size = fb_pitch * fb_height;

    fb = fb_back;


    for (uint32_t i = 0; i < buffer_size; i++)
        fb_back[i] = 0x00000000;


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
    uint32_t *src       = fb;
    uint64_t  qwords    = buffer_size / 2;   /* ننسخ 8 بايت في كل دورة */
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

uint32_t* fb_get_back_buffer()  { return fb; }        /* المخزن الذي يُرسم فيه */
uint32_t* fb_get_front_buffer() { return fb_front; }   /* ذاكرة الشاشة الحقيقية */
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

void draw_rect(
    int x,
    int y,
    int width,
    int height,
    uint32_t color
)
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

void fill_rect(
    int x,
    int y,
    int width,
    int height,
    uint32_t color
)
{
    for (int yy = 0; yy < height; yy++)
    {
        for (int xx = 0; xx < width; xx++)
        {
            fb_putpixel(
                x + xx,
                y + yy,
                color
            );
        }
    }
}

// رسم دائرة مجوفة
void draw_circle(int cx, int cy, int radius, uint32_t color)
{
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y)
    {
        // النقاط الثمانية المتماثلة
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

// رسم دائرة مملوءة
void fill_circle(int cx, int cy, int radius, uint32_t color)
{
    for (int y = -radius; y <= radius; y++)
    {
        int half_width = 0;
        // حساب عرض الدائرة عند هذا الارتفاع
        while (half_width * half_width + y * y < radius * radius)
        {
            half_width++;
        }
        half_width--; // آخر قيمة صالحة
        
        // رسم الخط الأفقي
        for (int x = -half_width; x <= half_width; x++)
        {
            fb_putpixel(cx + x, cy + y, color);
        }
    }
}



void draw_char(
    int x,
    int y,
    char c,
    uint32_t color
)
{
    if ((unsigned char)c >= 128)
        return;

    const uint8_t *glyph = fonts8x8[(uint8_t)c];

    for (int row = 0; row < 8; row++)
    {
        uint8_t bits = glyph[row];

        for (int col = 0; col < 8; col++)
        {
            if (bits & (1 << (7 - col)))
            {
                fb_putpixel(
                    x + col,
                    y + row,
                    color
                );
            }
        }
    }
}

void draw_string(
    int x,
    int y,
    const char *text,
    uint32_t color
)
{
    if (!text)
        return;

    while (*text)
    {
        if (*text == '\n')
        {
            x = 0;
            y += 8;
            text++;
            continue;
        }

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
