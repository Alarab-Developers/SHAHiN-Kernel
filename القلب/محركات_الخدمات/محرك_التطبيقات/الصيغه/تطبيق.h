#ifndef AROS_FORMAT_H
#define AROS_FORMAT_H

#include <stdint.h>

#include "القلب/محركات_الخدمات/محرك_التطبيقات/قيم_الملف.h"

/*
 * Header التطبيق
 */

typedef struct
{
    char format[128];

    uint64_t entry_point;

    uint64_t data_size;

} aros_header_t;

/*
 * تشغيل التطبيق
 */

int aros_run(file_t* f);

#endif
