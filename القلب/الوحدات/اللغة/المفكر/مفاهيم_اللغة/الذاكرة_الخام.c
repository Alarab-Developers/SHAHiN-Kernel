#include "الذاكرة_الخام.h"
#include "../../ادوات/ادوات.h"
#include "القلب/المكتبات/المكتبات.h"
#include "../المحلل.h"
#include "../../المنفذ/المنفذ.h"

int raw_memory_is_start(const char *line)
{
    if (!line)
        return 0;

    const char *p =
        skip_spaces(line);

    if (!starts_with(p, "ذاكرة_خام("))
        return 0;

    p =
        skip_prefix(
            p,
            "ذاكرة_خام("
        );

    p =
        skip_spaces(p);

    if (*p != ')')
        return 0;

    p++;

    p =
        skip_spaces(p);

    return *p == '\0';
}


int raw_memory_is_end(const char *line)
{
    if (!line)
        return 0;

    const char *p =
        skip_spaces(line);

    p =
        skip_spaces(p);

    return strcmp(
        p,
        "نهاية_ذاكرة"
    ) == 0;
}

/* ================================================================ */
/* دوال مساعدة خاصة بكل أمر (ثابتة على مستوى الملف)                 */
/* ================================================================ */

/* ذاكرة_اكتبN(عنوان, قيمة) */
static int handle_write(const char *p, const char *prefix, int bits)
{
    p = skip_prefix(p, prefix);
    p = skip_spaces(p);

    double address, value;
    if (parse_numeric_argument(&p, &address, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &value, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;

    switch (bits) {
        case 8:  return execute_memory_write8((uint64_t)address, (uint8_t)value);
        case 16: return execute_memory_write16((uint64_t)address, (uint16_t)value);
        case 32: return execute_memory_write((uint64_t)address, (uint32_t)value);
        case 64: return execute_memory_write64((uint64_t)address, (uint64_t)value);
        default: return -1;
    }
}

/* ذاكرة_اقراN(عنوان) */
static int handle_read(const char *p, const char *prefix, int bits, double *out)
{
    p = skip_prefix(p, prefix);
    p = skip_spaces(p);

    double address;
    if (parse_numeric_argument(&p, &address, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;

    switch (bits) {
        case 8: {
            uint8_t v;
            if (execute_memory_read8((uint64_t)address, &v) < 0) return -1;
            *out = (double)v;
            return 0;
        }
        case 16: {
            uint16_t v;
            if (execute_memory_read16((uint64_t)address, &v) < 0) return -1;
            *out = (double)v;
            return 0;
        }
        case 32: {
            uint32_t v;
            if (execute_memory_read((uint64_t)address, &v) < 0) return -1;
            *out = (double)v;
            return 0;
        }
        case 64: {
            uint64_t v;
            if (execute_memory_read64((uint64_t)address, &v) < 0) return -1;
            *out = (double)v;
            return 0;
        }
        default: return -1;
    }
}

/* ذاكرة_انسخ(هدف, مصدر, حجم) */
static int handle_copy(const char *p)
{
    p = skip_prefix(p, "ذاكرة_انسخ(");
    p = skip_spaces(p);

    double dest, src, size;
    if (parse_numeric_argument(&p, &dest, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &src, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &size, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    if (size < 0) return -1;

    return execute_memory_copy((uint64_t)dest, (uint64_t)src, (uint64_t)size);
}

/* ذاكرة_املأ(عنوان, قيمة, حجم) */
static int handle_set(const char *p)
{
    p = skip_prefix(p, "ذاكرة_املأ(");
    p = skip_spaces(p);

    double address, value, size;
    if (parse_numeric_argument(&p, &address, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &value, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &size, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    if (size < 0) return -1;

    return execute_memory_set((uint64_t)address, (uint8_t)value, (uint64_t)size);
}

/* ذاكرة_قارن(عنوان1, عنوان2, حجم) -> نتيجة (-1/0/1) */
static int handle_compare(const char *p, double *out)
{
    p = skip_prefix(p, "ذاكرة_قارن(");
    p = skip_spaces(p);

    double addr1, addr2, size;
    if (parse_numeric_argument(&p, &addr1, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &addr2, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &size, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    if (size < 0) return -1;

    int result;
    if (execute_memory_compare((uint64_t)addr1, (uint64_t)addr2, (uint64_t)size, &result) < 0) return -1;
    *out = (double)result;
    return 0;
}

/* ذاكرة_خصص(حجم) -> عنوان المُخصَّص (0 = فشل) */
static int handle_alloc(const char *p, double *out)
{
    p = skip_prefix(p, "ذاكرة_خصص(");
    p = skip_spaces(p);

    double size;
    if (parse_numeric_argument(&p, &size, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    if (size <= 0) return -1;

    uint64_t address;
    if (execute_memory_alloc((uint64_t)size, &address) < 0) return -1;
    *out = (double)address;
    return 0;
}

/* ذاكرة_حرر(عنوان) */
static int handle_free(const char *p)
{
    p = skip_prefix(p, "ذاكرة_حرر(");
    p = skip_spaces(p);

    double address;
    if (parse_numeric_argument(&p, &address, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    if (address == 0) return -1;

    return execute_memory_free((uint64_t)address);
}

/* ذاكرة_خصص_صفحة() -> عنوان الصفحة الفيزيائية */
static int handle_alloc_page(const char *p, double *out)
{
    p = skip_prefix(p, "ذاكرة_خصص_صفحة(");
    p = skip_spaces(p);
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;

    uint64_t address;
    if (execute_memory_alloc_page(&address) < 0) return -1;
    *out = (double)address;
    return 0;
}

/* ذاكرة_اربط(افتراضي, فيزيائي, صلاحيات) */
static int handle_map(const char *p)
{
    p = skip_prefix(p, "ذاكرة_اربط(");
    p = skip_spaces(p);

    double virt, phys, flags;
    if (parse_numeric_argument(&p, &virt, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &phys, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &flags, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;

    return execute_memory_map((uint64_t)virt, (uint64_t)phys, (uint64_t)flags);
}

/* ================================================================ */
/* موزّع الأوامر التي "تُرجع قيمة" (قابلة للإسناد)                  */
/* ================================================================ */
int raw_memory_dispatch_value(const char *p, double *value, int *matched)
{
    if (!p || !value || !matched) return -1;

    if (starts_with(p, "ذاكرة_اقرا8("))       { *matched = 1; return handle_read(p, "ذاكرة_اقرا8(", 8, value); }
    if (starts_with(p, "ذاكرة_اقرا16("))      { *matched = 1; return handle_read(p, "ذاكرة_اقرا16(", 16, value); }
    if (starts_with(p, "ذاكرة_اقرا32("))      { *matched = 1; return handle_read(p, "ذاكرة_اقرا32(", 32, value); }
    if (starts_with(p, "ذاكرة_اقرا64("))      { *matched = 1; return handle_read(p, "ذاكرة_اقرا64(", 64, value); }
    if (starts_with(p, "ذاكرة_قارن("))        { *matched = 1; return handle_compare(p, value); }
    if (starts_with(p, "ذاكرة_خصص_صفحة("))    { *matched = 1; return handle_alloc_page(p, value); }
    if (starts_with(p, "ذاكرة_خصص("))         { *matched = 1; return handle_alloc(p, value); }

    *matched = 0;
    return -1;
}

/* ================================================================ */
/* موزّع الأوامر التي تُستخدم كسطر مستقل (بدون إسناد)               */
/* ================================================================ */
int raw_memory_dispatch_statement(const char *p, int *matched)
{
    if (!p || !matched) return -1;

    if (starts_with(p, "ذاكرة_اكتب8("))  { *matched = 1; return handle_write(p, "ذاكرة_اكتب8(", 8); }
    if (starts_with(p, "ذاكرة_اكتب16(")) { *matched = 1; return handle_write(p, "ذاكرة_اكتب16(", 16); }
    if (starts_with(p, "ذاكرة_اكتب32(")) { *matched = 1; return handle_write(p, "ذاكرة_اكتب32(", 32); }
    if (starts_with(p, "ذاكرة_اكتب64(")) { *matched = 1; return handle_write(p, "ذاكرة_اكتب64(", 64); }

    if (starts_with(p, "ذاكرة_انسخ("))   { *matched = 1; return handle_copy(p); }
    if (starts_with(p, "ذاكرة_املأ("))   { *matched = 1; return handle_set(p); }
    if (starts_with(p, "ذاكرة_حرر("))    { *matched = 1; return handle_free(p); }
    if (starts_with(p, "ذاكرة_اربط("))   { *matched = 1; return handle_map(p); }

    /* أوامر القراءة/الترجيع يمكن استخدامها كسطر مستقل؛ تُهمَل قيمتها */
    if (starts_with(p, "ذاكرة_اقرا8(")  || starts_with(p, "ذاكرة_اقرا16(") ||
        starts_with(p, "ذاكرة_اقرا32(") || starts_with(p, "ذاكرة_اقرا64(") ||
        starts_with(p, "ذاكرة_قارن(")   || starts_with(p, "ذاكرة_خصص_صفحة(") ||
        starts_with(p, "ذاكرة_خصص(")) {
        double value;
        int value_matched = 0;
        int res = raw_memory_dispatch_value(p, &value, &value_matched);
        *matched = value_matched;
        return res;
    }

    *matched = 0;
    return -1;
}
