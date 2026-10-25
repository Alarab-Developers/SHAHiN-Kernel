#include "الحلقات.h"
#include "../ادوات/ادوات.h"


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
