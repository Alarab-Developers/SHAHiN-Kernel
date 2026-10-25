#include "التصريح_بالأنواع.h"
#include "../ادوات/ادوات.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"

int declaration_is_start(const char *line, arp_type_t *out_type,
                         char **out_var_name, const char **out_rhs)
{
    if (!line || !out_type || !out_var_name || !out_rhs) return 0;
    *out_type = ARP_TYPE_UNKNOWN;
    *out_var_name = NULL;
    *out_rhs = NULL;

    const char *p = skip_spaces(line);
    if (*p == '\0') return 0;

    const char *kw_start = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    int kw_len = p - kw_start;
    if (kw_len <= 0) return 0;

    char *kw = arp_strndup(kw_start, kw_len);
    if (!kw) return 0;

    arp_type_t t = arp_type_from_keyword(kw);
    memory_api.free(kw);
    if (t == ARP_TYPE_UNKNOWN) return 0;

    p = skip_spaces(p);
    if (*p == '\0') return 0;

    /* اقرأ اسم المتغيّر (حتى '=' أو فراغ أو '(') */
    const char *ns = p;
    while (*p && *p != '=' && *p != ' ' && *p != '\t' && *p != '(') p++;
    int nlen = p - ns;
    if (nlen <= 0) return 0;

    p = skip_spaces(p);
    if (*p != '=') return 0;
    p = skip_spaces(p + 1);
    if (*p == '\0') return 0;

    char *vname = arp_strndup(ns, nlen);
    if (!vname) return 0;

    *out_type = t;
    *out_var_name = vname;
    *out_rhs = p;
    return 1;
}

int declaration_is_array_start(const char *line,
                               arp_type_t *out_type,
                               char **out_var_name,
                               int *out_dims, int *out_ndims,
                               const char **out_rhs)
{
    if (!line || !out_type || !out_var_name || !out_dims ||
        !out_ndims || !out_rhs) return 0;
    *out_var_name = NULL;
    *out_rhs = NULL;
    *out_ndims = 0;
    for (int i = 0; i < 8; i++) out_dims[i] = 0;

    const char *p = skip_spaces(line);
    if (*p == '\0') return 0;

    /* اقرأ كلمة النوع */
    const char *kws = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '(') p++;
    int kw_len = p - kws;
    if (kw_len <= 0) return 0;

    char *kw = arp_strndup(kws, kw_len);
    if (!kw) return 0;
    arp_type_t t = arp_type_from_keyword(kw);
    memory_api.free(kw);
    if (t == ARP_TYPE_UNKNOWN) return 0;

    p = skip_spaces(p);
    if (*p == '\0') return 0;

    /* اقرأ اسم المتغير حتى أول '(' */
    const char *ns = p;
    while (*p && *p != '(' && *p != ' ' && *p != '\t') p++;
    int nlen = p - ns;
    if (nlen <= 0) return 0;

    p = skip_spaces(p);
    if (*p != '(') return 0;   /* ليس مصفوفة */

    char *vname = arp_strndup(ns, nlen);
    if (!vname) return 0;

    /* اقرأ الأبعاد: (n1)(n2)... */
    int ndims = 0;
    int dims[8];
    while (*p == '(')
    {
        p++;
        p = skip_spaces(p);
        int n = 0, has = 0;
        while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; has = 1; }
        if (!has || n <= 0) goto fail;
        if (ndims >= 8) goto fail;
        dims[ndims++] = n;
        p = skip_spaces(p);
        if (*p != ')') goto fail;
        p++;
        p = skip_spaces(p);
    }

    if (ndims == 0) goto fail;

    if (*p != '=') goto fail;
    p = skip_spaces(p + 1);
    if (*p != '{') goto fail;

    *out_type = t;
    *out_var_name = vname;
    *out_ndims = ndims;
    for (int i = 0; i < ndims; i++) out_dims[i] = dims[i];
    *out_rhs = p;
    return 1;

fail:
    memory_api.free(vname);
    return 0;
}

void declaration_free(char *var_name)
{
    if (var_name) memory_api.free(var_name);
}
