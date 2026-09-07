#ifndef التعليقات_H
#define التعليقات_H

#include "القلب/المكتبات/انواع.h"

/*
 * إزالة التعليقات من السكربت.
 * الصيغة: "" ... ""
 * تُرجع 0 عند النجاح، -1 عند وجود تعليق غير مغلق.
 * cleaned يجب تحريره بواسطة المتصل بعد الاستخدام.
 */
int comment_remove(
    const char *script,
    uint32_t size,
    char **cleaned,
    uint32_t *cleaned_size
);

#endif
