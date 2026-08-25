#ifndef المنفذ_H
#define المنفذ_H

#include "القلب/المكتبات/انواع.h"

/*
 * تنفيذ أوامر للغة
 */



/*
 * تنفيذ background
 */
int execute_background(
    uint32_t color
);



int execute_calculate(
    double result
);

/*
 * تشغيل تطبيق
 */
int execute_app(
    const char *name
);

// الرسم


int execute_rect(
    int x,
    int y,
    int width,
    int height,
    uint32_t color
);
#endif
