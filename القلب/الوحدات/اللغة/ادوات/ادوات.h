#ifndef ادوات_H
#define ادوات_H

#include "القلب/المكتبات/انواع.h"

/* ================================================================ */
/* قراءة سطر من السكربت – تخصيص ديناميكي                           */
/* ================================================================ */
int read_line(
    const char *script,
    uint32_t size,
    uint32_t *offset,
    char **line_ptr,      // سيتم تخصيصه داخلياً (يُحرر بواسطة المتصل)
    uint32_t *line_len
);

const char *skip_spaces(const char *p);
int starts_with(const char *text, const char *prefix);
const char *skip_prefix(const char *p, const char *prefix);
const char *find_char(const char *text, char target);
void double_to_string(double value, char *buffer, int precision);
double string_to_double(const char *text, const char **end);

#endif
