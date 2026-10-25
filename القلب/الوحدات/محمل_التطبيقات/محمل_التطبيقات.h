#ifndef LODER_H
#define LODER_H
#include "القلب/المكتبات/المكتبات.h"


/*
 * اسم التطبيق الرئيسي.
 *
 * التطبيقات الآن عبارة عن سكربتات ARP فقط.
 */
#define AUTO_APP_NAME "test.تطبيق"


/*
 * تشغيل ملف تطبيق.
 *
 * ترجع:
 *
 * 0  عند النجاح
 * -1 عند حدوث خطأ
 */
int loder_run_script(
    const char* name
);



int loder_start_script(
    const char* name
);



void loder_auto_start(void);


int loader_app(const char* name);


#endif
