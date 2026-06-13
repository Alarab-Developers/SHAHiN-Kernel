#include "بوابة_النواه.h"
#include "محرك_تواصل_العمليات/محرك_تواصل_العمليات.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مؤقت_تنظيم_المقاطعات/المؤقت.h"
#include "القلب/محركات_الخدمات/محرك_الفيديو/بوابة_الفيديو.h"
#include "القلب/محركات_الخدمات/محرك_الفيديو/محرك_الفيديو.h"
#include "محرك_المقاطعات/بوابة_المقاطعات.h"
#include "القلب/محركات_الخدمات/محرك_التطبيقات/الصيغه/هيكل_التطبيق/أمتداد_التطبيق.h"
#include "القلب/محركات_الخدمات/محرك_التطبيقات/قيم_الملف.h"
#include "القلب/محركات_الخدمات/محرك_التطبيقات/محرك_التطبيقات.h"

extern file_t* find_file(const char* name);

/* PORT IO */
static void api_outb(uint16_t port, uint8_t value)
{
    interrupt_api.outb(port, value);
}

static uint8_t api_inb(uint16_t port)
{
    return interrupt_api.inb(port);
}

/* TIMER */
static uint64_t api_timer_ticks()
{
    volatile uint64_t* t = (volatile uint64_t*)&ticks;
    return *t;
}

extern int mouse_x;
extern int mouse_y;

static int api_mouse_get_x() { return mouse_x; }
static int api_mouse_get_y() { return mouse_y; }

/* VIDEO */
static uint32_t* api_video_map()    { return fb_get_back_buffer(); }
static uint32_t  api_screen_width() { return video_api.width();    }
static uint32_t  api_screen_height(){ return video_api.height();   }
static uint32_t  api_screen_pitch() { return video_api.pitch();    }
static void      api_video_present(){ fb_swap_buffers();           }


static void api_swap()
{
    video_api.swap();
}

/* LAUNCH APP */
static int api_launch_app(const char* name)
{
    file_t* f = find_file(name);
    if (!f) return -1;
    app_run(f);
    return 0;
}

/* API TABLE */
kernel_api_t kapi = {
    .ipc_send      = ipc_send,
    .ipc_receive   = ipc_receive,
    .outb          = api_outb,
    .inb           = api_inb,
    .timer_ticks   = api_timer_ticks,
    .mouse_get_x   = api_mouse_get_x,
    .mouse_get_y   = api_mouse_get_y,
    .video_map     = api_video_map,
    .screen_width  = api_screen_width,
    .screen_height = api_screen_height,
    .screen_pitch  = api_screen_pitch,
    .video_present = api_video_present,
    .swap          = api_swap,
    .launch_app    = api_launch_app,
};
