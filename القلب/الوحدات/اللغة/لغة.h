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
int arp_run_library(
    const char *script,
    uint32_t size
);


#endif
