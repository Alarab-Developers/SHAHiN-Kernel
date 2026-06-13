#ifndef أمتداد_التطبيق_H
#define أمتداد_التطبيق_H

#include "../تطبيق.h"

/*
 * امتداد/صيغة التطبيق
 */

#define APP_FORMAT "[ت]"

/*
 * التحقق من صيغة الملف
 */

int app_format_valid(
    file_t* f
);

#endif
