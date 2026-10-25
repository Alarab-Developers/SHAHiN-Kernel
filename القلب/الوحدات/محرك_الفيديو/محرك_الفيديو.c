#include "محرك_الفيديو.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"

static uint32_t *fb_back_dynamic = NULL;
static uint32_t  fb_back_fallback[1024 * 768];

static uint32_t *fb          = NULL;
static uint32_t *fb_front    = NULL;
static uint32_t  fb_width    = 0;
static uint32_t  fb_height   = 0;
static uint32_t  fb_pitch    = 0;
static uint32_t  buffer_size = 0;

static int g_has_avx2 = 0;

/* =========================================================
 * كشف دعم AVX2
 * ========================================================= */

static void fb_detect_cpu(void)
{
    uint32_t eax, ebx, ecx, edx;

    __asm__ volatile ("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1));
    int has_avx     = (ecx >> 28) & 1;
    int has_osxsave = (ecx >> 27) & 1;

    if (!has_avx || !has_osxsave) { g_has_avx2 = 0; return; }

    __asm__ volatile ("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(7), "c"(0));
    g_has_avx2 = (ebx & (1 << 5)) ? 1 : 0;

    if (g_has_avx2)
    {
        uint32_t xcr0_lo, xcr0_hi;
        __asm__ volatile ("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
        xcr0_lo |= 0x6;
        __asm__ volatile ("xsetbv" : : "a"(xcr0_lo), "d"(xcr0_hi), "c"(0));
    }
}

/* =========================================================
 * التهيئة
 * ========================================================= */

void fb_init_from_boot(boot_info_t *boot)
{
    if (!boot || !boot->FrameBuffer) return;

    fb_detect_cpu();

    fb_front  = (uint32_t *)boot->FrameBuffer;
    fb_width  = boot->Width;
    fb_height = boot->Height;
    fb_pitch  = boot->PixelsPerScanLine;

    if (!fb_pitch) { fb_front = NULL; return; }

    buffer_size = fb_pitch * fb_height;

    if (fb_back_dynamic) {
        memory_api.free(fb_back_dynamic);
        fb_back_dynamic = NULL;
    }

    fb_back_dynamic = (uint32_t *)memory_api.alloc(
        buffer_size * sizeof(uint32_t));

    if (fb_back_dynamic) {
        fb = fb_back_dynamic;
    } else {
        fb = fb_back_fallback;
        if (buffer_size > sizeof(fb_back_fallback) / sizeof(uint32_t)) {
            buffer_size = sizeof(fb_back_fallback) / sizeof(uint32_t);
            fb_height = buffer_size / fb_pitch;
            if (fb_height > boot->Height) fb_height = boot->Height;
            buffer_size = fb_pitch * fb_height;
        }
    }

    /* تفريغ */
    uint64_t *p64 = (uint64_t *)fb;
    for (uint32_t i = 0; i < buffer_size / 2; i++) p64[i] = 0;

    fb_swap_buffers();
}

/* =========================================================
 * مسح
 * ========================================================= */

void fb_clear(uint32_t color)
{
    if (!fb || !buffer_size) return;

    if (g_has_avx2)
    {
        __asm__ volatile (
            "vmovd %0, %%xmm0\n\t"
            "vpbroadcastd %%xmm0, %%ymm0\n\t"
            :
            : "r"(color)
            : "xmm0", "ymm0"
        );

        uint32_t i = 0;
        while (i < buffer_size && (((uintptr_t)(fb + i)) & 31))
            fb[i++] = color;

        for (; i + 32 <= buffer_size; i += 32)
        {
            __asm__ volatile (
                "vmovntdq %%ymm0,    (%0)\n\t"
                "vmovntdq %%ymm0,  32(%0)\n\t"
                "vmovntdq %%ymm0,  64(%0)\n\t"
                "vmovntdq %%ymm0,  96(%0)\n\t"
                :
                : "r"(fb + i)
                : "memory"
            );
        }

        __asm__ volatile ("sfence" ::: "memory");

        for (; i < buffer_size; i++) fb[i] = color;
        __asm__ volatile ("vzeroupper" ::: "memory");
        return;
    }

    uint64_t pattern = ((uint64_t)color << 32) | color;
    uint64_t *p64 = (uint64_t *)fb;
    uint32_t qwords = buffer_size / 2;
    for (uint32_t i = 0; i < qwords; i++) p64[i] = pattern;
    if (buffer_size & 1) fb[buffer_size - 1] = color;
}

/* =========================================================
 * تبديل البافر
 * ========================================================= */

void fb_swap_buffers(void)
{
    if (!fb_front || !fb || !buffer_size) return;

    if (g_has_avx2 &&
        ((((uintptr_t)fb_front) & 31) == 0) &&
        ((((uintptr_t)fb)       & 31) == 0))
    {
        size_t n = buffer_size;
        size_t i = 0;

        for (; i + 32 <= n; i += 32)
        {
            __asm__ volatile (
                "vmovdqu    (%1), %%ymm0\n\t"
                "vmovdqu  32(%1), %%ymm1\n\t"
                "vmovdqu  64(%1), %%ymm2\n\t"
                "vmovdqu  96(%1), %%ymm3\n\t"
                "vmovntdq %%ymm0,    (%0)\n\t"
                "vmovntdq %%ymm1,  32(%0)\n\t"
                "vmovntdq %%ymm2,  64(%0)\n\t"
                "vmovntdq %%ymm3,  96(%0)\n\t"
                :
                : "r"(fb_front + i), "r"(fb + i)
                : "ymm0", "ymm1", "ymm2", "ymm3", "memory"
            );
        }

        for (; i + 8 <= n; i += 8)
        {
            __asm__ volatile (
                "vmovdqu    (%1), %%ymm0\n\t"
                "vmovntdq %%ymm0, (%0)\n\t"
                :
                : "r"(fb_front + i), "r"(fb + i)
                : "ymm0", "memory"
            );
        }

        __asm__ volatile ("sfence" ::: "memory");

        for (; i < n; i++) fb_front[i] = fb[i];
        __asm__ volatile ("vzeroupper" ::: "memory");
        return;
    }

    uint64_t qwords = (uint64_t)buffer_size / 2;
    uint32_t rem    = buffer_size & 1;
    uint32_t *dst   = fb_front;
    uint32_t *src   = fb;

    __asm__ volatile (
        "cld\n\t rep movsq"
        : "+D"(dst), "+S"(src), "+c"(qwords)
        : : "memory"
    );

    if (rem) dst[0] = src[0];
}

/* =========================================================
 * استعلامات
 * ========================================================= */

uint32_t *fb_get_back_buffer(void)  { return fb; }
uint32_t *fb_get_front_buffer(void) { return fb_front; }
uint32_t  fb_get_width(void)        { return fb_width; }
uint32_t  fb_get_height(void)       { return fb_height; }
uint32_t  fb_get_pitch(void)        { return fb_pitch; }
uint32_t  fb_get_buffer_size(void)  { return buffer_size; }
