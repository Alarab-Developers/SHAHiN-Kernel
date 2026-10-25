#ifndef لغة_H
#define لغة_H

#include "القلب/المكتبات/انواع.h"
#include "القلب/المكتبات/المكتبات.h"

typedef struct
{
    int (*run_line)(const char *line);
    int (*run_script)(const char *script, uint32_t size);

} arp_api_t;

/*
 * بوابة اللغة
 */
extern arp_api_t arp_api;

/*
 * تشغيل مكتبة ARP
 *
 * بدون حذف الدوال المسجلة
 */
int arp_run_library(const char *script, uint32_t size);

/*
 * تحرير سياق ARP كامل (دوال + متغيرات + الغلاف).
 *
 * يُستدعى من مدير العمليات عند خروج أي عملية ARP
 * لمنع تسرب الذاكرة.
 *
 * ctx_ptr: مؤشر arp_context_t (يُمرَّر كـ void* لتفادي include دائري).
 */
void arp_context_cleanup(void *ctx_ptr);

/*
 * arp_compile_only — تجميع AOT
 *
 * يُحلّل السكربت ويُسجّل الدوال والهياكل والمتغيرات في السياق
 * الحالي دون تنفيذ أي سطر. يُستخدم من arp_aot_compile() لجمع
 * الدوال قبل ترجمتها إلى ملف تنفيذي.
 */
int arp_compile_only(const char *script, uint32_t size);

#endif
