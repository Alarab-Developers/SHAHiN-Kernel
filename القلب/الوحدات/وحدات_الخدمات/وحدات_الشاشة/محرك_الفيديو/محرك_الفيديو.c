#include "القلب/الوحدات/وحدات_الخدمات/وحدات_الشاشة/محرك_الفيديو/محرك_الفيديو.h"

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

    /*
     * تنبيه مهم: كان الكود القديم يقصّ buffer_size فقط عند تجاوز
     * السعة، بينما تبقى fb_width/fb_height/fb_pitch كما وصلت من
     * boot info دون تغيير. المشكلة أن fb_putpixel (وكل ما يعتمد
     * عليها من draw_char/draw_string/draw_rect/fill_rect) تستخدم
     * fb_height/fb_pitch غير المقصوصتين في فحص الحدود والفهرسة:
     *
     *     if ((uint32_t)y >= fb_height) return;
     *     fb[y * fb_pitch + x] = color;
     *
     * فإذا كانت pitch * height الفعلية (كما يعطيها GOP على بعض
     * المحاكيات/الأجهزة، خصوصًا QEMU/OVMF حيث PixelsPerScanLine
     * غالبًا أكبر من العرض الحقيقي بسبب محاذاة الذاكرة) أكبر من
     * FB_MAX_BUFFER_PIXELS، فإن fb_putpixel كانت تكتب خارج حدود
     * مصفوفة fb_back الثابتة فعليًا → تخريب ذاكرة BSS المجاورة →
     * غالبًا page fault لا يوجد له معالج → triple fault → إعادة
     * تشغيل تلقائية للنواة. هذا لا يظهر بالضرورة على كل محاكي لأن
     * كل واحد يعطي دقة/pitch مختلفة عبر GOP.
     *
     * الإصلاح: نقصّ fb_height نفسها (وليس buffer_size فقط) بحيث
     * تبقى fb_height * fb_pitch <= FB_MAX_BUFFER_PIXELS دائمًا،
     * فيتوافق فحص الحدود في fb_putpixel مع الحجم الحقيقي للمخزن.
     */
    if ((uint64_t)fb_pitch * fb_height > FB_MAX_BUFFER_PIXELS)
    {
        fb_height = FB_MAX_BUFFER_PIXELS / fb_pitch;
    }

    /* احتياط إضافي: تأكد أن العرض المعلَن لا يتجاوز الـ pitch نفسه،
     * وإلا فإن x القادم من draw_string/draw_rect قد يتجاوز نهاية
     * كل سطر ضمن نفس الصف التالي في المخزن. */
    if (fb_width > fb_pitch)
    {
        fb_width = fb_pitch;
    }

    buffer_size = fb_pitch * fb_height;

    fb = fb_back;

    /* تصفير المخزن الخلفي حتى لا تظهر بيانات عشوائية من مرحلة الإقلاع */
    for (uint32_t i = 0; i < buffer_size; i++)
        fb_back[i] = 0x00000000;

    /* عرض الإطار الأول فارغًا فورًا بدل ترك محتوى UEFI القديم على الشاشة */
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

/*
 * هذه هي الدالة الأهم: نسخ دفعي واحد وسريع من المخزن الخلفي
 * إلى ذاكرة الشاشة الحقيقية. طالما هذا يحدث دفعة واحدة، لن يرى
 * المستخدم أي رسم جزئي أو وميض — فقط الإطار النهائي الكامل.
 */
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

/*
 * ملاحظة هامة: هذه الدالة تكتب مباشرة إلى ذاكرة الشاشة الحقيقية،
 * متجاوزة المخزن الخلفي تمامًا. بعد تفعيل نظام المخزن المزدوج
 * (بعد استدعاء fb_init_from_boot)، أي استدعاء لهذه الدالة سيُمحى
 * فورًا عند أول fb_swap_buffers() تالٍ — لأن المخزن الخلفي لا يعرف
 * عن هذا التغيير. استخدمها فقط لتصحيح أخطاء مبكر جدًا قبل تهيئة
 * الفيديو، أو تجنبها كليًا واستخدم fb_clear() + fb_swap_buffers().
 */
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
