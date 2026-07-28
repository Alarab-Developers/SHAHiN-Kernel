#ifndef بيانات_التطبيق_H
#define بيانات_التطبيق_H

#include <stdint.h>

#include "../تطبيق.h"

/*
 * بداية قسم البيانات داخل الملف
 */

void* app_data_ptr(
    file_t* f
);

/*
 * حجم قسم البيانات
 */

uint64_t app_data_size(
    file_t* f
);

/*
 * نسخ البيانات إلى الذاكرة
 */

void app_data_copy(
    file_t* f,
    void* dst
);

#endif
