#ifndef المفسر_H
#define المفسر_H

#include "مفاهيم_اللغة/الدوال.h"
#include "القلب/المكتبات/المكتبات.h"

extern int arp_loop_depth;   // عمق تداخل الحلقات

/*
 * عمق تداخل استدعاءات الدوال
 *
 * 0   : التنفيذ يجري على مستوى السكربت الرئيسي (خارج أي دالة تماماً)
 * >0  : التنفيذ يجري داخل جسم دالة واحدة أو أكثر (متداخلة)
 *
 * تُستخدم في arp_execute_line لمنع أي أمر (اطبع، مستطيل، خلفية،
 * ذاكرة_*، إسناد متغير، استمر/اعد/كسر/ارجع، ...) من العمل خارج
 * الدوال؛ الاستثناءان الوحيدان المسموحان عند 0 هما استدعاء مكتبة
 * واستدعاء دالة معرّفة.
 */
extern int arp_function_depth;

extern double arp_return_value;   
extern int arp_return_flag;

/*
 * نوع القيمة التي ترجعها الدالة عبر ارجع(...)
 *
 * ARP_RETURN_NONE   : ارجع() بدون قيمة (أو لم تُنفَّذ ارجع() إطلاقاً)
 * ARP_RETURN_NUMBER : ارجع(قيمة رقمية)   -> arp_return_value
 * ARP_RETURN_TEXT   : ارجع("نص")         -> arp_return_text
 */
#define ARP_RETURN_NONE   0
#define ARP_RETURN_NUMBER 1
#define ARP_RETURN_TEXT   2

extern int arp_return_type;
extern char arp_return_text[256];

int arp_execute_line(const char *line);
int arp_execute_lines(const char **lines, int line_count);
int parse_numeric_argument(const char **p, double *value, int last);
int parse_function_arguments(const char *p, double **arguments_out, int *argument_count);
int parse_hardware(const char *p);

/*
 * استدعاء دالة مسجّلة باسمها
 *
 * name       : اسم الدالة
 * args_start : مؤشر إلى ما بعد القوس المفتوح "(" مباشرة
 *
 * ترجع نفس رموز arp_execute_lines:
 *  0  : انتهت الدالة بدون ارجع()
 * -1  : خطأ
 * -2 / -3 : تسرب استمر()/كسر() من داخل الدالة (حالة غير طبيعية)
 * -4  : الدالة نفّذت ارجع(...) — القيمة (إن وجدت) في
 *       arp_return_value / arp_return_text حسب arp_return_type
 */
int arp_invoke_function(const char *name, const char *args_start);

#endif
