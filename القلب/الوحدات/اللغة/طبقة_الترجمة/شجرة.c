#include "شجرة.h"
#include "../ادوات/ادوات.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"

struct arp_array
{
    arp_type_t  elem_type;   /* نوع العناصر */
    int         ndims;       /* عدد الأبعاد */
    int         dims[8];     /* أحجام كل بُعد */
    int         total;       /* حجم مسطح */
    arp_value_t *items;      /* مسطّحة row-major */
    int         refcount;    /* ★ جديد: عدّاد مراجع للنسخ السطحي */
};

/* =========================================================
 * ★★★ arp_value_t — إدارة القيم ★★★
 * ========================================================= */

void arp_value_init(arp_value_t *v, arp_type_t t)
{
    if (!v) return;
    memset(v, 0, sizeof(*v));
    v->type = t;
}


void arp_value_free(arp_value_t *v)
{
    if (!v) return;
    if (v->type == ARP_TYPE_TEXT && v->as.text)
    {
        memory_api.free(v->as.text);
        v->as.text = NULL;
    }
    else if (v->type == ARP_TYPE_ARRAY && v->as.array)
    {
        arp_array_release(v->as.array);   /* ★ refcount بدل free المباشر */
        v->as.array = NULL;
    }
    v->type = ARP_TYPE_UNKNOWN;
}

int arp_value_copy(arp_value_t *dst, const arp_value_t *src)
{
    if (!dst || !src) return -1;
    if (dst == src) return 0;

    arp_value_free(dst);
    dst->type = src->type;

    if (src->type == ARP_TYPE_TEXT && src->as.text)
    {
        dst->as.text = arp_strdup(src->as.text);
        if (!dst->as.text) { dst->type = ARP_TYPE_UNKNOWN; return -1; }
    }
    else if (src->type == ARP_TYPE_ARRAY && src->as.array)
    {
        /* ★ نسخة سطحية مع refcount: تشارك نفس المصفوفة */
        arp_array_retain(src->as.array);
        dst->as.array = src->as.array;
    }
    else
    {
        dst->as = src->as;
    }
    return 0;
}

void arp_value_make_int(arp_value_t *v, int64_t x)
{
    arp_value_init(v, ARP_TYPE_INT);
    v->as.i64 = x;
}

void arp_value_make_float(arp_value_t *v, float x)
{
    arp_value_init(v, ARP_TYPE_FLOAT);
    v->as.f32 = x;
}

void arp_value_make_double(arp_value_t *v, double x)
{
    arp_value_init(v, ARP_TYPE_DOUBLE);
    v->as.f64 = x;
}

void arp_value_make_char(arp_value_t *v, uint32_t cp)
{
    arp_value_init(v, ARP_TYPE_CHAR);
    v->as.cp = cp;
}

void arp_value_make_bool(arp_value_t *v, int b)
{
    arp_value_init(v, ARP_TYPE_BOOL);
    v->as.b = b ? 1 : 0;
}

void arp_value_make_ptr(arp_value_t *v, uintptr_t p)
{
    arp_value_init(v, ARP_TYPE_PTR);
    v->as.ptr = p;
}

int arp_value_make_text(arp_value_t *v, const char *s, int len)
{
    if (!v) return -1;
    arp_value_free(v);
    v->type = ARP_TYPE_TEXT;
    if (!s) { v->as.text = arp_strdup(""); return v->as.text ? 0 : -1; }
    if (len < 0) len = (int)strlen(s);
    v->as.text = arp_strndup(s, len);
    return v->as.text ? 0 : -1;
}

/* =========================================================
 * التحويلات الصريحة
 * ========================================================= */

int arp_value_to_i64(const arp_value_t *v, int64_t *out)
{
    if (!v || !out) return -1;
    switch (v->type)
    {
        case ARP_TYPE_INT:    *out = v->as.i64; return 0;
        case ARP_TYPE_FLOAT:  *out = (int64_t)v->as.f32; return 0;
        case ARP_TYPE_DOUBLE: *out = (int64_t)v->as.f64; return 0;
        case ARP_TYPE_CHAR:   *out = (int64_t)v->as.cp; return 0;
        case ARP_TYPE_BOOL:   *out = (int64_t)v->as.b; return 0;
        case ARP_TYPE_PTR:    *out = (int64_t)v->as.ptr; return 0;
        case ARP_TYPE_TEXT:
        {
            if (!v->as.text) return -1;
            const char *end = NULL;
            double d = string_to_double(v->as.text, &end);
            if (!end || *end != '\0') return -1;
            *out = (int64_t)d;
            return 0;
        }
        default: return -1;
    }
}

int arp_value_to_f32(const arp_value_t *v, float *out)
{
    if (!v || !out) return -1;
    switch (v->type)
    {
        case ARP_TYPE_INT:    *out = (float)v->as.i64; return 0;
        case ARP_TYPE_FLOAT:  *out = v->as.f32; return 0;
        case ARP_TYPE_DOUBLE: *out = (float)v->as.f64; return 0;
        case ARP_TYPE_CHAR:   *out = (float)v->as.cp; return 0;
        case ARP_TYPE_BOOL:   *out = (float)v->as.b; return 0;
        case ARP_TYPE_PTR:    *out = (float)v->as.ptr; return 0;
        case ARP_TYPE_TEXT:
        {
            if (!v->as.text) return -1;
            const char *end = NULL;
            double d = string_to_double(v->as.text, &end);
            if (!end || *end != '\0') return -1;
            *out = (float)d;
            return 0;
        }
        default: return -1;
    }
}

int arp_value_to_f64(const arp_value_t *v, double *out)
{
    if (!v || !out) return -1;
    switch (v->type)
    {
        case ARP_TYPE_INT:    *out = (double)v->as.i64; return 0;
        case ARP_TYPE_FLOAT:  *out = (double)v->as.f32; return 0;
        case ARP_TYPE_DOUBLE: *out = v->as.f64; return 0;
        case ARP_TYPE_CHAR:   *out = (double)v->as.cp; return 0;
        case ARP_TYPE_BOOL:   *out = (double)v->as.b; return 0;
        case ARP_TYPE_PTR:    *out = (double)v->as.ptr; return 0;
        case ARP_TYPE_TEXT:
        {
            if (!v->as.text) return -1;
            const char *end = NULL;
            double d = string_to_double(v->as.text, &end);
            if (!end || *end != '\0') return -1;
            *out = d;
            return 0;
        }
        default: return -1;
    }
}

int arp_value_to_text(const arp_value_t *v, char **out)
{
    if (!v || !out) return -1;
    *out = NULL;

    switch (v->type)
    {
        case ARP_TYPE_TEXT:
            *out = arp_strdup(v->as.text ? v->as.text : "");
            return *out ? 0 : -1;

        case ARP_TYPE_INT:
        {
            char buf[32];
            double_to_string((double)v->as.i64, buf, 0);
            *out = arp_strdup(buf);
            return *out ? 0 : -1;
        }

        case ARP_TYPE_FLOAT:
        case ARP_TYPE_DOUBLE:
        {
            char buf[64];
            double d = (v->type == ARP_TYPE_FLOAT) ? (double)v->as.f32 : v->as.f64;
            double_to_string(d, buf, 6);
            *out = arp_strdup(buf);
            return *out ? 0 : -1;
        }

        case ARP_TYPE_CHAR:
        {
            uint32_t cp = v->as.cp;
            char buf[5];
            int n = 0;
            if (cp < 0x80) { buf[n++] = (char)cp; }
            else if (cp < 0x800) {
                buf[n++] = (char)(0xC0 | (cp >> 6));
                buf[n++] = (char)(0x80 | (cp & 0x3F));
            }
            else if (cp < 0x10000) {
                buf[n++] = (char)(0xE0 | (cp >> 12));
                buf[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                buf[n++] = (char)(0x80 | (cp & 0x3F));
            }
            else {
                buf[n++] = (char)(0xF0 | (cp >> 18));
                buf[n++] = (char)(0x80 | ((cp >> 12) & 0x3F));
                buf[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                buf[n++] = (char)(0x80 | (cp & 0x3F));
            }
            buf[n] = '\0';
            *out = arp_strdup(buf);
            return *out ? 0 : -1;
        }

        case ARP_TYPE_BOOL:
            *out = arp_strdup(v->as.b ? "1" : "0");
            return *out ? 0 : -1;

        case ARP_TYPE_PTR:
        {
            char buf[32];
            uintptr_t p = v->as.ptr;
            static const char hex[] = "0123456789abcdef";
            char tmp[20];
            int n = 0;
            if (p == 0) tmp[n++] = '0';
            else {
                while (p && n < (int)sizeof(tmp) - 1) {
                    tmp[n++] = hex[p & 0xF];
                    p >>= 4;
                }
            }
            buf[0] = '0'; buf[1] = 'x';
            int k = 2;
            for (int i = n - 1; i >= 0; i--) buf[k++] = tmp[i];
            buf[k] = '\0';
            *out = arp_strdup(buf);
            return *out ? 0 : -1;
        }

        default: return -1;
    }
}

/* =========================================================
 * الترقية والـ coercion
 * ========================================================= */

static int type_rank(arp_type_t t)
{
    switch (t) {
        case ARP_TYPE_BOOL:   return 1;
        case ARP_TYPE_CHAR:   return 2;
        case ARP_TYPE_INT:    return 3;
        case ARP_TYPE_FLOAT:  return 4;
        case ARP_TYPE_DOUBLE: return 5;
        case ARP_TYPE_TEXT:   return 10;
        case ARP_TYPE_PTR:    return 20;
        default:              return 0;
    }
}

static arp_type_t promote(arp_type_t a, arp_type_t b)
{
    if (a == ARP_TYPE_UNKNOWN) return b;
    if (b == ARP_TYPE_UNKNOWN) return a;
    if (a == b) return a;

    if (a == ARP_TYPE_TEXT || b == ARP_TYPE_TEXT)
        return (a == ARP_TYPE_TEXT && b == ARP_TYPE_TEXT) ? ARP_TYPE_TEXT
                                                          : ARP_TYPE_UNKNOWN;

    if (a == ARP_TYPE_PTR && b == ARP_TYPE_INT) return ARP_TYPE_PTR;
    if (b == ARP_TYPE_PTR && a == ARP_TYPE_INT) return ARP_TYPE_PTR;

    return type_rank(a) >= type_rank(b) ? a : b;
}

int arp_value_coerce(const arp_value_t *src, arp_value_t *dst)
{
    if (!src || !dst) return -1;

    arp_type_t target = dst->type;
    if (target == ARP_TYPE_UNKNOWN) return arp_value_copy(dst, src);
    if (src->type == target) return arp_value_copy(dst, src);

    switch (target)
    {
        case ARP_TYPE_INT:
        {
            int64_t x;
            if (arp_value_to_i64(src, &x) < 0) return -1;
            arp_value_make_int(dst, x);
            return 0;
        }
        case ARP_TYPE_FLOAT:
        {
            float x;
            if (arp_value_to_f32(src, &x) < 0) return -1;
            arp_value_make_float(dst, x);
            return 0;
        }
        case ARP_TYPE_DOUBLE:
        {
            double x;
            if (arp_value_to_f64(src, &x) < 0) return -1;
            arp_value_make_double(dst, x);
            return 0;
        }
        case ARP_TYPE_CHAR:
        {
            if (src->type == ARP_TYPE_INT)
            {
                arp_value_make_char(dst, (uint32_t)src->as.i64);
                return 0;
            }
            if (src->type == ARP_TYPE_CHAR)
            {
                arp_value_make_char(dst, src->as.cp);
                return 0;
            }
            return -1;
        }
        case ARP_TYPE_TEXT:
        {
            char *s = NULL;
            if (arp_value_to_text(src, &s) < 0) return -1;
            arp_value_free(dst);
            dst->type = ARP_TYPE_TEXT;
            dst->as.text = s;
            return 0;
        }
        case ARP_TYPE_BOOL:
        {
            if (src->type == ARP_TYPE_BOOL) { arp_value_make_bool(dst, src->as.b); return 0; }
            int64_t x = 0;
            if (arp_value_to_i64(src, &x) < 0) return -1;
            arp_value_make_bool(dst, x != 0);
            return 0;
        }
        case ARP_TYPE_PTR:
        {
            if (src->type == ARP_TYPE_PTR) { arp_value_make_ptr(dst, src->as.ptr); return 0; }
            int64_t x = 0;
            if (arp_value_to_i64(src, &x) < 0) return -1;
            arp_value_make_ptr(dst, (uintptr_t)x);
            return 0;
        }
        default: return -1;
    }
}

int arp_value_is_assignable(arp_type_t declared, arp_type_t actual)
{
    if (declared == actual) return 1;
    if (declared == ARP_TYPE_UNKNOWN) return 1;

    switch (declared)
    {
        case ARP_TYPE_DOUBLE:
            return (actual == ARP_TYPE_INT || actual == ARP_TYPE_FLOAT ||
                    actual == ARP_TYPE_CHAR || actual == ARP_TYPE_BOOL);
        case ARP_TYPE_FLOAT:
            return (actual == ARP_TYPE_INT || actual == ARP_TYPE_CHAR ||
                    actual == ARP_TYPE_BOOL);
        case ARP_TYPE_INT:
            return (actual == ARP_TYPE_BOOL || actual == ARP_TYPE_CHAR);
        case ARP_TYPE_TEXT:
            return (actual == ARP_TYPE_TEXT || actual == ARP_TYPE_CHAR);
        case ARP_TYPE_BOOL:
            return (actual == ARP_TYPE_BOOL || actual == ARP_TYPE_INT);
        case ARP_TYPE_CHAR:
            return (actual == ARP_TYPE_CHAR || actual == ARP_TYPE_INT);
        case ARP_TYPE_PTR:
            return (actual == ARP_TYPE_PTR || actual == ARP_TYPE_INT);
        default: return 0;
    }
}

/* =========================================================
 * العمليات الحسابية النوعية
 * ========================================================= */

int arp_value_add(const arp_value_t *a, const arp_value_t *b, arp_value_t *out)
{
    if (!a || !b || !out) return -1;

    if (a->type == ARP_TYPE_TEXT && b->type == ARP_TYPE_TEXT)
    {
        const char *sa = a->as.text ? a->as.text : "";
        const char *sb = b->as.text ? b->as.text : "";
        size_t la = strlen(sa), lb = strlen(sb);
        char *buf = (char *)memory_api.alloc(la + lb + 1);
        if (!buf) return -1;
        memcpy(buf, sa, la);
        memcpy(buf + la, sb, lb);
        buf[la + lb] = '\0';
        arp_value_free(out);
        out->type = ARP_TYPE_TEXT;
        out->as.text = buf;
        return 0;
    }

    arp_type_t t = promote(a->type, b->type);
    if (t == ARP_TYPE_UNKNOWN) return -1;

    switch (t)
    {
        case ARP_TYPE_INT:
        {
            int64_t x, y;
            if (arp_value_to_i64(a, &x) < 0) return -1;
            if (arp_value_to_i64(b, &y) < 0) return -1;
            int64_t r;
            if (__builtin_add_overflow(x, y, &r))
            {
                arp_value_make_double(out, (double)x + (double)y);
            }
            else arp_value_make_int(out, r);
            return 0;
        }
        case ARP_TYPE_FLOAT:
        {
            float x, y;
            if (arp_value_to_f32(a, &x) < 0) return -1;
            if (arp_value_to_f32(b, &y) < 0) return -1;
            arp_value_make_float(out, x + y);
            return 0;
        }
        case ARP_TYPE_DOUBLE:
        {
            double x, y;
            if (arp_value_to_f64(a, &x) < 0) return -1;
            if (arp_value_to_f64(b, &y) < 0) return -1;
            arp_value_make_double(out, x + y);
            return 0;
        }
        case ARP_TYPE_PTR:
        {
            int64_t x, y;
            if (arp_value_to_i64(a, &x) < 0) return -1;
            if (arp_value_to_i64(b, &y) < 0) return -1;
            arp_value_make_ptr(out, (uintptr_t)(x + y));
            return 0;
        }
        default: return -1;
    }
}

int arp_value_sub(const arp_value_t *a, const arp_value_t *b, arp_value_t *out)
{
    if (!a || !b || !out) return -1;
    arp_type_t t = promote(a->type, b->type);
    if (t == ARP_TYPE_UNKNOWN || t == ARP_TYPE_TEXT) return -1;

    switch (t)
    {
        case ARP_TYPE_INT:
        {
            int64_t x, y, r;
            if (arp_value_to_i64(a, &x) < 0) return -1;
            if (arp_value_to_i64(b, &y) < 0) return -1;
            if (__builtin_sub_overflow(x, y, &r))
                arp_value_make_double(out, (double)x - (double)y);
            else arp_value_make_int(out, r);
            return 0;
        }
        case ARP_TYPE_FLOAT:
        {
            float x, y;
            if (arp_value_to_f32(a, &x) < 0) return -1;
            if (arp_value_to_f32(b, &y) < 0) return -1;
            arp_value_make_float(out, x - y);
            return 0;
        }
        case ARP_TYPE_DOUBLE:
        {
            double x, y;
            if (arp_value_to_f64(a, &x) < 0) return -1;
            if (arp_value_to_f64(b, &y) < 0) return -1;
            arp_value_make_double(out, x - y);
            return 0;
        }
        case ARP_TYPE_PTR:
        {
            int64_t x, y;
            if (arp_value_to_i64(a, &x) < 0) return -1;
            if (arp_value_to_i64(b, &y) < 0) return -1;
            arp_value_make_ptr(out, (uintptr_t)(x - y));
            return 0;
        }
        default: return -1;
    }
}

int arp_value_mul(const arp_value_t *a, const arp_value_t *b, arp_value_t *out)
{
    if (!a || !b || !out) return -1;
    arp_type_t t = promote(a->type, b->type);
    if (t == ARP_TYPE_UNKNOWN || t == ARP_TYPE_TEXT) return -1;

    switch (t)
    {
        case ARP_TYPE_INT:
        {
            int64_t x, y, r;
            if (arp_value_to_i64(a, &x) < 0) return -1;
            if (arp_value_to_i64(b, &y) < 0) return -1;
            if (__builtin_mul_overflow(x, y, &r))
                arp_value_make_double(out, (double)x * (double)y);
            else arp_value_make_int(out, r);
            return 0;
        }
        case ARP_TYPE_FLOAT:
        {
            float x, y;
            if (arp_value_to_f32(a, &x) < 0) return -1;
            if (arp_value_to_f32(b, &y) < 0) return -1;
            arp_value_make_float(out, x * y);
            return 0;
        }
        case ARP_TYPE_DOUBLE:
        {
            double x, y;
            if (arp_value_to_f64(a, &x) < 0) return -1;
            if (arp_value_to_f64(b, &y) < 0) return -1;
            arp_value_make_double(out, x * y);
            return 0;
        }
        default: return -1;
    }
}

int arp_value_div(const arp_value_t *a, const arp_value_t *b, arp_value_t *out)
{
    if (!a || !b || !out) return -1;
    arp_type_t t = promote(a->type, b->type);
    if (t == ARP_TYPE_UNKNOWN || t == ARP_TYPE_TEXT) return -1;

    switch (t)
    {
        case ARP_TYPE_INT:
        {
            int64_t x, y;
            if (arp_value_to_i64(a, &x) < 0) return -1;
            if (arp_value_to_i64(b, &y) < 0) return -1;
            if (y == 0) return -1;
            if (x < 0 && -x == x && y == -1) return -1;
            arp_value_make_int(out, x / y);
            return 0;
        }
        case ARP_TYPE_FLOAT:
        {
            float x, y;
            if (arp_value_to_f32(a, &x) < 0) return -1;
            if (arp_value_to_f32(b, &y) < 0) return -1;
            arp_value_make_float(out, x / y);
            return 0;
        }
        case ARP_TYPE_DOUBLE:
        {
            double x, y;
            if (arp_value_to_f64(a, &x) < 0) return -1;
            if (arp_value_to_f64(b, &y) < 0) return -1;
            arp_value_make_double(out, x / y);
            return 0;
        }
        default: return -1;
    }
}

int arp_value_mod(const arp_value_t *a, const arp_value_t *b, arp_value_t *out)
{
    if (!a || !b || !out) return -1;
    arp_type_t t = promote(a->type, b->type);
    if (t == ARP_TYPE_UNKNOWN || t == ARP_TYPE_TEXT) return -1;

    switch (t)
    {
        case ARP_TYPE_INT:
        {
            int64_t x, y;
            if (arp_value_to_i64(a, &x) < 0) return -1;
            if (arp_value_to_i64(b, &y) < 0) return -1;
            if (y == 0) return -1;
            if (x < 0 && -x == x && y == -1) {
                arp_value_make_int(out, 0);
                return 0;
            }
            arp_value_make_int(out, x % y);
            return 0;
        }
        case ARP_TYPE_FLOAT:
        {
            float x, y;
            if (arp_value_to_f32(a, &x) < 0) return -1;
            if (arp_value_to_f32(b, &y) < 0) return -1;
            if (y == 0.0f) return -1;
            float q = x / y;
            int64_t qi = (int64_t)q;
            arp_value_make_float(out, x - (float)qi * y);
            return 0;
        }
        case ARP_TYPE_DOUBLE:
        {
            double x, y;
            if (arp_value_to_f64(a, &x) < 0) return -1;
            if (arp_value_to_f64(b, &y) < 0) return -1;
            if (y == 0.0) return -1;
            double q = x / y;
            int64_t qi = (int64_t)q;
            arp_value_make_double(out, x - (double)qi * y);
            return 0;
        }
        default: return -1;
    }
}

int arp_value_shl(const arp_value_t *a, const arp_value_t *b, arp_value_t *out)
{
    if (!a || !b || !out) return -1;
    int64_t x, y;
    if (arp_value_to_i64(a, &x) < 0) return -1;
    if (arp_value_to_i64(b, &y) < 0) return -1;
    if (y < 0 || y > 63) return -1;
    arp_value_make_int(out, (int64_t)((uint64_t)x << (uint32_t)y));
    return 0;
}

int arp_value_shr(const arp_value_t *a, const arp_value_t *b, arp_value_t *out)
{
    if (!a || !b || !out) return -1;
    int64_t x, y;
    if (arp_value_to_i64(a, &x) < 0) return -1;
    if (arp_value_to_i64(b, &y) < 0) return -1;
    if (y < 0 || y > 63) return -1;
    arp_value_make_int(out, x >> (uint32_t)y);
    return 0;
}

int arp_value_bit_and(const arp_value_t *a, const arp_value_t *b, arp_value_t *out)
{
    if (!a || !b || !out) return -1;
    int64_t x, y;
    if (arp_value_to_i64(a, &x) < 0) return -1;
    if (arp_value_to_i64(b, &y) < 0) return -1;
    arp_value_make_int(out, x & y);
    return 0;
}

int arp_value_bit_or(const arp_value_t *a, const arp_value_t *b, arp_value_t *out)
{
    if (!a || !b || !out) return -1;
    int64_t x, y;
    if (arp_value_to_i64(a, &x) < 0) return -1;
    if (arp_value_to_i64(b, &y) < 0) return -1;
    arp_value_make_int(out, x | y);
    return 0;
}

int arp_value_bit_xor(const arp_value_t *a, const arp_value_t *b, arp_value_t *out)
{
    if (!a || !b || !out) return -1;
    int64_t x, y;
    if (arp_value_to_i64(a, &x) < 0) return -1;
    if (arp_value_to_i64(b, &y) < 0) return -1;
    arp_value_make_int(out, x ^ y);
    return 0;
}


int arp_value_neg(const arp_value_t *a, arp_value_t *out)
{
    if (!a || !out) return -1;
    switch (a->type)
    {
        case ARP_TYPE_INT:    arp_value_make_int(out, -a->as.i64); return 0;
        case ARP_TYPE_FLOAT:  arp_value_make_float(out, -a->as.f32); return 0;
        case ARP_TYPE_DOUBLE: arp_value_make_double(out, -a->as.f64); return 0;
        case ARP_TYPE_BOOL:   arp_value_make_int(out, -(int64_t)a->as.b); return 0;
        case ARP_TYPE_CHAR:   arp_value_make_int(out, -(int64_t)a->as.cp); return 0;
        default: return -1;
    }
}

int arp_value_truthy(const arp_value_t *v)
{
    if (!v) return 0;
    switch (v->type) {
        case ARP_TYPE_BOOL:   return v->as.b ? 1 : 0;
        case ARP_TYPE_INT:    return v->as.i64 != 0;
        case ARP_TYPE_FLOAT:  return v->as.f32 != 0.0f;
        case ARP_TYPE_DOUBLE: return v->as.f64 != 0.0;
        case ARP_TYPE_CHAR:   return v->as.cp != 0;
        case ARP_TYPE_PTR:    return v->as.ptr != 0;
        case ARP_TYPE_TEXT:   return (v->as.text && v->as.text[0] != '\0');
        case ARP_TYPE_ARRAY:  return (v->as.array && v->as.array->total > 0);
        default:              return 0;
    }
}

int arp_value_cmp(int op, const arp_value_t *a, const arp_value_t *b, int *out)
{
    if (!a || !b || !out) return -1;

    arp_type_t t = promote(a->type, b->type);
    if (t == ARP_TYPE_UNKNOWN) return -1;

    int cmp = 0;

    switch (t)
    {
        case ARP_TYPE_INT:
        {
            int64_t x, y;
            if (arp_value_to_i64(a, &x) < 0) return -1;
            if (arp_value_to_i64(b, &y) < 0) return -1;
            cmp = (x < y) ? -1 : (x > y) ? 1 : 0;
            break;
        }
        case ARP_TYPE_FLOAT:
        {
            float x, y;
            if (arp_value_to_f32(a, &x) < 0) return -1;
            if (arp_value_to_f32(b, &y) < 0) return -1;
            cmp = (x < y) ? -1 : (x > y) ? 1 : 0;
            break;
        }
        case ARP_TYPE_DOUBLE:
        {
            double x, y;
            if (arp_value_to_f64(a, &x) < 0) return -1;
            if (arp_value_to_f64(b, &y) < 0) return -1;
            cmp = (x < y) ? -1 : (x > y) ? 1 : 0;
            break;
        }
        case ARP_TYPE_TEXT:
        {
            const char *sa = a->as.text ? a->as.text : "";
            const char *sb = b->as.text ? b->as.text : "";
            cmp = strcmp(sa, sb);
            break;
        }
        case ARP_TYPE_PTR:
        {
            int64_t x, y;
            if (arp_value_to_i64(a, &x) < 0) return -1;
            if (arp_value_to_i64(b, &y) < 0) return -1;
            cmp = (x < y) ? -1 : (x > y) ? 1 : 0;
            break;
        }
        default: return -1;
    }

    switch (op)
    {
        case AST_CMP_EQ:  *out = (cmp == 0); break;
        case AST_CMP_NEQ: *out = (cmp != 0); break;
        case AST_CMP_LT:  *out = (cmp <  0); break;
        case AST_CMP_LTE: *out = (cmp <= 0); break;
        case AST_CMP_GT:  *out = (cmp >  0); break;
        case AST_CMP_GTE: *out = (cmp >= 0); break;
        default: return -1;
    }
    return 0;
}

/* =========================================================
 * جدول أسماء الأنواع
 * ========================================================= */

static const struct {
    const char *name;
    arp_type_t  type;
} g_type_table[] = {
    { "صحيح",       ARP_TYPE_INT    },
    { "عشري_صحيح",  ARP_TYPE_FLOAT  },
    { "عشري_دقيق",  ARP_TYPE_DOUBLE },
    { "حرف",        ARP_TYPE_CHAR   },
    { "نص",         ARP_TYPE_TEXT   },
    { "منطقي",      ARP_TYPE_BOOL   },
    { "مؤشر",       ARP_TYPE_PTR    },
};

arp_type_t arp_type_from_keyword(const char *kw)
{
    if (!kw) return ARP_TYPE_UNKNOWN;
    for (size_t i = 0; i < sizeof(g_type_table)/sizeof(g_type_table[0]); i++)
        if (strcmp(kw, g_type_table[i].name) == 0)
            return g_type_table[i].type;
    return ARP_TYPE_UNKNOWN;
}

int arp_is_type_keyword(const char *kw)
{
    return arp_type_from_keyword(kw) != ARP_TYPE_UNKNOWN;
}

const char *arp_type_name(arp_type_t t)
{
    switch (t) {
        case ARP_TYPE_INT:     return "صحيح";
        case ARP_TYPE_FLOAT:   return "عشري_صحيح";
        case ARP_TYPE_DOUBLE:  return "عشري_دقيق";
        case ARP_TYPE_CHAR:    return "حرف";
        case ARP_TYPE_TEXT:    return "نص";
        case ARP_TYPE_BOOL:    return "منطقي";
        case ARP_TYPE_PTR:     return "مؤشر";
        case ARP_TYPE_ARRAY:   return "مصفوفة";
        default:               return "غير_معروف";
    }
}

const char *arp_type_name_ar(arp_type_t t)
{
    return arp_type_name(t);
}

/* =========================================================
 * عقد AST
 * ========================================================= */

static ast_node_t *ast_create(ast_node_type_t type)
{
    ast_node_t *node = (ast_node_t *)memory_api.alloc(sizeof(ast_node_t));
    if (!node) return NULL;
    memset(node, 0, sizeof(ast_node_t));
    node->type = type;
    return node;
}

ast_node_t *ast_number(double value)
{
    ast_node_t *n = ast_create(AST_NUMBER);
    if (!n) return NULL;
    n->number = value;
    return n;
}

ast_node_t *ast_string(const char *text)
{
    ast_node_t *n;
    if (!text) return NULL;
    n = ast_create(AST_STRING);
    if (!n) return NULL;
    n->text = arp_strdup(text);
    if (!n->text) { memory_api.free(n); return NULL; }
    return n;
}

ast_node_t *ast_variable(const char *name)
{
    ast_node_t *n;
    if (!name) return NULL;
    n = ast_create(AST_VARIABLE);
    if (!n) return NULL;
    n->name = arp_strdup(name);
    if (!n->name) { memory_api.free(n); return NULL; }
    return n;
}

ast_node_t *ast_assignment(const char *name, ast_node_t *value)
{
    ast_node_t *n;
    if (!name || !*name || !value) return NULL;
    n = ast_create(AST_ASSIGN);
    if (!n) return NULL;
    n->assignment.name = arp_strdup(name);
    if (!n->assignment.name) { memory_api.free(n); return NULL; }
    n->assignment.value = value;
    return n;
}

ast_node_t *ast_function_call(const char *name, ast_node_t **args, int arg_count)
{
    ast_node_t *n;
    if (!name || arg_count < 0) return NULL;
    n = ast_create(AST_FUNCTION_CALL);
    if (!n) return NULL;
    n->function_call.name = arp_strdup(name);
    if (!n->function_call.name) { memory_api.free(n); return NULL; }
    n->function_call.args = args;
    n->function_call.arg_count = arg_count;
    return n;
}

ast_node_t *ast_binary(ast_node_type_t type, ast_node_t *left, ast_node_t *right)
{
    if (type != AST_ADD && type != AST_SUB && type != AST_MUL && type != AST_DIV &&
        type != AST_MOD && type != AST_SHL && type != AST_SHR &&
        type != AST_BIT_AND && type != AST_BIT_OR && type != AST_BIT_XOR &&
        type != AST_CMP_EQ && type != AST_CMP_NEQ &&
        type != AST_CMP_LT && type != AST_CMP_LTE &&
        type != AST_CMP_GT && type != AST_CMP_GTE &&
        type != AST_AND && type != AST_OR)
        return NULL;
    ast_node_t *n = ast_create(type);
    if (!n) return NULL;
    n->binary.left  = left;
    n->binary.right = right;
    return n;
}

ast_node_t *ast_unary(ast_node_type_t type, ast_node_t *operand)
{
    if (type != AST_NEG) return NULL;
    ast_node_t *n = ast_create(type);
    if (!n) return NULL;
    n->unary.operand = operand;
    return n;
}

ast_node_t *ast_index(const char *name, ast_node_t **indices, int nidx)
{
    if (!name || !indices || nidx <= 0) return NULL;
    ast_node_t *n = ast_create(AST_INDEX);
    if (!n) return NULL;
    n->array_index.name = arp_strdup(name);
    if (!n->array_index.name) { memory_api.free(n); return NULL; }
    n->array_index.indices = indices;
    n->array_index.index_count = nidx;
    return n;
}

ast_node_t *ast_array_lit(ast_node_t **elems, int n)
{
    if (n < 0) return NULL;
    ast_node_t *node = ast_create(AST_ARRAY_LIT);
    if (!node) return NULL;
    node->array_lit.elements = elems;
    node->array_lit.element_count = n;
    return node;
}

const char *ast_type_name(ast_node_type_t type)
{
    switch (type) {
        case AST_NUMBER:        return "NUMBER";
        case AST_STRING:        return "STRING";
        case AST_VARIABLE:      return "VARIABLE";
        case AST_FUNCTION_CALL: return "FUNCTION_CALL";
        case AST_ADD:           return "ADD";
        case AST_SUB:           return "SUB";
        case AST_MUL:           return "MUL";
        case AST_DIV:           return "DIV";
        case AST_MOD:           return "MOD";
        case AST_SHL:           return "SHL";
        case AST_SHR:           return "SHR";
        case AST_BIT_AND:       return "BIT_AND";
        case AST_BIT_OR:        return "BIT_OR";
        case AST_BIT_XOR:       return "BIT_XOR";
        case AST_NEG:           return "NEG";
        case AST_ASSIGN:        return "ASSIGN";
        case AST_CMP_EQ:        return "CMP_EQ";
        case AST_CMP_NEQ:       return "CMP_NEQ";
        case AST_CMP_LT:        return "CMP_LT";
        case AST_CMP_LTE:       return "CMP_LTE";
        case AST_CMP_GT:        return "CMP_GT";
        case AST_CMP_GTE:       return "CMP_GTE";
        case AST_AND:           return "AND";
        case AST_OR:            return "OR";
        case AST_INDEX:         return "INDEX";
        case AST_ARRAY_LIT:     return "ARRAY_LIT";
        default:                return "UNKNOWN";
    }
}

void ast_free(ast_node_t *node)
{
    if (!node) return;

    switch (node->type)
    {
        case AST_NUMBER: break;

        case AST_STRING:
            if (node->text) memory_api.free(node->text);
            break;

        case AST_VARIABLE:
            if (node->name) memory_api.free(node->name);
            break;

        case AST_ASSIGN:
            if (node->assignment.name) memory_api.free(node->assignment.name);
            ast_free(node->assignment.value);
            break;

        case AST_FUNCTION_CALL:
            if (node->function_call.name) memory_api.free(node->function_call.name);
            if (node->function_call.args)
            {
                for (int i = 0; i < node->function_call.arg_count; i++)
                    ast_free(node->function_call.args[i]);
                memory_api.free(node->function_call.args);
            }
            break;

        case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV:
        case AST_MOD: case AST_SHL: case AST_SHR:
        case AST_BIT_AND: case AST_BIT_OR: case AST_BIT_XOR:
        case AST_CMP_EQ: case AST_CMP_NEQ:
        case AST_CMP_LT: case AST_CMP_LTE:
        case AST_CMP_GT: case AST_CMP_GTE:
        case AST_AND: case AST_OR:
            ast_free(node->binary.left);
            ast_free(node->binary.right);
            break;

        case AST_INDEX:
            if (node->array_index.name) memory_api.free(node->array_index.name);
            if (node->array_index.indices)
            {
                for (int i = 0; i < node->array_index.index_count; i++)
                    ast_free(node->array_index.indices[i]);
                memory_api.free(node->array_index.indices);
            }
            break;

        case AST_ARRAY_LIT:
            if (node->array_lit.elements)
            {
                for (int i = 0; i < node->array_lit.element_count; i++)
                    ast_free(node->array_lit.elements[i]);
                memory_api.free(node->array_lit.elements);
            }
            break;
        default: break;
    }
    memory_api.free(node);
}

/* =========================================================
 * ★★★ المصفوفات ★★★
 * ========================================================= */

arp_array_t *arp_array_create(arp_type_t elem_type, int dims)
{
    (void)dims;
    arp_array_t *a = (arp_array_t *)memory_api.alloc(sizeof(arp_array_t));
    if (!a) return NULL;
    memset(a, 0, sizeof(*a));
    a->elem_type = elem_type;
    a->ndims     = 0;
    a->refcount  = 1;         /* ★ المالك الأول */
    return a;
}

/* ★ زيادة المرجع — تُستدعى عند نسخ قيمة تحمل مصفوفة */
int arp_array_retain(arp_array_t *a)
{
    if (!a) return -1;
    a->refcount++;
    return 0;
}

/* ★ إنقاص المرجع، وتحرير فعلي عند الوصول إلى صفر */
void arp_array_release(arp_array_t *a)
{
    if (!a) return;
    a->refcount--;
    if (a->refcount > 0) return;

    if (a->items)
    {
        for (int i = 0; i < a->total; i++)
            arp_value_free(&a->items[i]);
        memory_api.free(a->items);
    }
    memory_api.free(a);
}

/* توافق مع الواجهة القديمة: التحرير المباشر = إنقاص مرجع */
void arp_array_free(arp_array_t *a)
{
    arp_array_release(a);
}

int arp_array_dim_count(const arp_array_t *a)
{
    return a ? a->ndims : 0;
}

int arp_array_dim_size(const arp_array_t *a, int dim)
{
    if (!a || dim < 0 || dim >= a->ndims) return -1;
    return a->dims[dim];
}

static int array_flat_index(const arp_array_t *a, const int *idx, int nidx)
{
    if (!a || nidx != a->ndims) return -1;
    int off = 0;
    for (int i = 0; i < nidx; i++)
    {
        if (idx[i] < 0 || idx[i] >= a->dims[i]) return -1;
        off = off * a->dims[i] + idx[i];
    }
    return off;
}

int arp_array_set(arp_array_t *a, const int *indices, int nidx,
                  const arp_value_t *value)
{
    int off = array_flat_index(a, indices, nidx);
    if (off < 0) return -1;
    arp_value_free(&a->items[off]);
    return arp_value_copy(&a->items[off], value);
}

int arp_array_get(const arp_array_t *a, const int *indices, int nidx,
                  arp_value_t *out)
{
    int off = array_flat_index(a, indices, nidx);
    if (off < 0) return -1;
    return arp_value_copy(out, &a->items[off]);
}

int arp_array_fill_from_literal(arp_array_t *a,
                                const arp_value_t *flat_values,
                                int total)
{
    if (!a || !flat_values || total != a->total) return -1;

    if (!a->items)
    {
        a->items = (arp_value_t *)memory_api.alloc(total * sizeof(arp_value_t));
        if (!a->items) return -1;
    }

    for (int i = 0; i < total; i++)
    {
        arp_value_free(&a->items[i]);
        if (arp_value_copy(&a->items[i], &flat_values[i]) < 0)
        {
            for (int j = 0; j <= i; j++) arp_value_free(&a->items[j]);
            memory_api.free(a->items);
            a->items = NULL;
            a->total = 0;
            return -1;
        }
    }
    a->total = total;
    return 0;
}

int arp_array_init(arp_array_t *a, int ndims, const int *dims,
                   const arp_value_t *flat_values, int total)
{
    if (!a || !dims || ndims <= 0 || ndims > 8) return -1;
    if (total < 0) return -1;
    if (total > 0 && !flat_values) return -1;

    int expected = 1;
    for (int i = 0; i < ndims; i++)
    {
        if (dims[i] <= 0) return -1;
        expected *= dims[i];
    }
    if (expected != total) return -1;

    a->ndims = ndims;
    for (int i = 0; i < ndims; i++) a->dims[i] = dims[i];
    a->total = total;

    if (a->items) { memory_api.free(a->items); a->items = NULL; }

    if (total > 0)
    {
        a->items = (arp_value_t *)memory_api.alloc((size_t)total * sizeof(arp_value_t));
        if (!a->items) { a->total = 0; return -1; }

        for (int i = 0; i < total; i++)
        {
            arp_value_init(&a->items[i], ARP_TYPE_UNKNOWN);
            if (arp_value_copy(&a->items[i], &flat_values[i]) < 0)
            {
                for (int j = 0; j <= i; j++) arp_value_free(&a->items[j]);
                memory_api.free(a->items);
                a->items = NULL;
                a->total = 0;
                return -1;
            }
        }
    }
    return 0;
}

int arp_value_make_array(arp_value_t *v, arp_array_t *arr)
{
    if (!v || !arr) return -1;
    arp_value_free(v);
    v->type = ARP_TYPE_ARRAY;
    v->as.array = arr;
    return 0;
}
