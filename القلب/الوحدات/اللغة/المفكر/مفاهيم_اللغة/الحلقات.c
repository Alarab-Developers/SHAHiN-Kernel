#include "الحلقات.h"
#include "الدوال.h"
#include "../../ادوات/ادوات.h"
#include "../المحلل.h"

int loop_is_start(const char *line)
{
    if (!line) return 0;
    const char *p = skip_spaces(line);
    if (!starts_with(p, "حلقة(")) return 0;
    p = skip_prefix(p, "حلقة(");
    p = skip_spaces(p);
    if (*p != ')') return 0;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return 0;
    return 1;
}

int loop_is_end(const char *line)
{
    if (!line) return 0;
    const char *p = skip_spaces(line);
    if (!starts_with(p, "نهاية_الحلقة")) return 0;
    p = skip_prefix(p, "نهاية_الحلقة");
    p = skip_spaces(p);
    if (*p != '\0') return 0;
    return 1;
}

int loop_execute(const char **lines, int line_count)
{
    if (!lines || line_count < 0) return -1;
    while (1)
    {
        arp_loop_depth++;
        int result = arp_execute_lines(lines, line_count);
        arp_loop_depth--;

        if (result == -4) return -4;      // إضافة: إرجاع قيمة
        if (result == -2) continue;              // continue
        else if (result == -3) return 0;        // break
                               // خروج من الحلقة
        else if (result < 0) return -1;
                             // خطأ
        // result == 0 → حلقة لا نهائية، نستمر
    }
    return 0;
}
