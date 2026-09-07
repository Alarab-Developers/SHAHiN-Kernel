#include "سياق_اللغة.h"

#include "محرك_العمليات/بوابة_العمليات.h"
#include "القلب/الوحدات/شاشة_التطوير/شاشة_التطوير.h"

arp_context_t* arp_get_current_context(void)
{
    process_t* p =
        process_api.current();

    if (!p)
    {
        //development_screen_println(
          //  "APP: current process NULL"
        //);

        //development_screen_update();

        while (1)
            asm volatile ("hlt");
    }


    if (
        !process_api.get_app_context(p)
    )
    {
        //development_screen_println(
        //    "APP: context NULL"
        //);

       // development_screen_update();

        while (1)
            asm volatile ("hlt");
    }

    //development_screen_println(
    //    "APP: context OK"
    //);

   // development_screen_update();

    if (!p)
        return 0;

    return (arp_context_t*)
        process_api.get_app_context(p);
}
