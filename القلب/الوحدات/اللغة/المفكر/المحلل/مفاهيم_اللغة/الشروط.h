#ifndef الشروط_H
#define الشروط_H

#include "القلب/المكتبات/المكتبات.h"

/*
 * تقييم شرط:
 *
 * اذا(س = 2)
 *
 * ترجع:
 *
 * 1 = صحيح
 * 0 = خطأ
 * -1 = خطأ في بناء الشرط
 */
int condition_evaluate(
    const char *condition
);


/*
 * التحقق هل السطر بداية شرط
 */
int condition_is_start(
    const char *line
);


int condition_is_else(
    const char *line
);


/*
 * التحقق هل السطر نهاية شرط
 */
int condition_is_end(
    const char *line
);


#endif
