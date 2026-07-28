#include <stdint.h>

#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مُتحكم_المقاطعات.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مؤقت_تنظيم_المقاطعات/المؤقت.h"
#include "محرك_الجدولة/محرك_الجدولة.h"
#include "محرك_الجدولة/مُدير_الأحداث/مُدير_الأحداث.h"

volatile uint64_t ticks = 0;

void timer_init()
{
    lapic_timer_init();
}

uint64_t timer_get_ticks()
{
    return ticks;
}

void timer_handler()
{
    ticks++;

    scheduler_tick();

    events |= EVENT_SCHEDULE;
}
