#ifndef شروط_لوحة_المفاتيح_H
#define شروط_لوحة_المفاتيح_H
#include "القلب/المكتبات/المكتبات.h"

/*
 * دالة تقييم شرط لوحة المفاتيح.
 * تُستدعى من condition_evaluate في الشروط.c
 * مثال: keyboard_condition("لوحة_المفاتيح(↑)")
 * ترجع: 1 إذا كان المفتاح مضغوطاً، 0 إن لم يكن، -1 عند الخطأ.
 */
int keyboard_condition(const char *p);

#endif
