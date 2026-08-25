#ifndef الدوال_H
#define الدوال_H


#define ARP_MAX_FUNCTIONS 32
#define ARP_MAX_FUNCTION_LINES 512
#define ARP_MAX_LINE_LENGTH 512

/*
 * الحد الأقصى لمعاملات الدالة
 */
#define ARP_MAX_PARAMETERS 16

#include "القلب/المكتبات/المكتبات.h"


typedef struct
{
    /*
     * اسم الدالة
     */
    char name[64];


    /*
     * أسماء المعاملات
     *
     * مثال:
     *
     * دالة(جمع, أ, ب)
     *
     * parameters[0] = أ
     * parameters[1] = ب
     */
    char parameters[
        ARP_MAX_PARAMETERS
    ][
        64
    ];


    /*
     * عدد المعاملات
     */
    int parameter_count;


    /*
     * جسم الدالة
     */
    char lines[
        ARP_MAX_FUNCTION_LINES
    ][
        ARP_MAX_LINE_LENGTH
    ];


    int line_count;

} arp_function_t;


/*
 * تهيئة جدول الدوال
 */
void functions_init(void);


/*
 * تسجيل دالة
 */
int function_register(
    const char *name,
    const char parameters[][64],
    int parameter_count,
    const char **lines,
    int line_count
);


/*
 * البحث عن دالة
 */
arp_function_t *function_find(
    const char *name
);


#endif
