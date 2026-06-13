#ifndef رأس_التطبيق_H
#define رأس_التطبيق_H

#include "محرك_تواصل_العمليات/بوابة_النواه.h"

typedef void (*app_entry_t)(
    kernel_api_t*
);

#define MAX_APPS 8

/*
 * حجز slot جديد
 */

int app_allocate_slot(void);

/*
 * تعيين نقطة الدخول للتطبيق
 */

void app_set_entry(
    int slot,
    app_entry_t entry
);

/*
 * جلب launcher الخاص بالـ slot
 */

void (*app_get_launcher(int slot))(void);

#endif
