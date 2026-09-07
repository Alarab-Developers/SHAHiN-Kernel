#include "مؤشر.h"
#include "القلب/المكتبات/المكتبات.h"
#include "القلب/الوحدات/محرك_الفيديو/بوابة_الفيديو.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مكتبة_واجهة_الإدخال_والإخراج.h"

static int cycle = 0;
static uint8_t packet[3];

static uint32_t color = 0x00FF00;

void mouse_init_test(void)
{
    outb(0x64, 0xA8);

    outb(0x64, 0xD4);
    outb(0x60, 0xF4);
}

void mouse_handler_test(void)
{
    packet[cycle] = inb(0x60);

    cycle++;

    if (cycle < 3)
        return;

    cycle = 0;

    int8_t dx = (int8_t)packet[1];
    int8_t dy = (int8_t)packet[2];

    /*
     * لا نغير اللون إلا عند وجود حركة فعلية
     */
    if (dx != 0 || dy != 0)
    {
        color += 0x000100;

        video_api.clear(color);
        video_api.swap();
    }
}
