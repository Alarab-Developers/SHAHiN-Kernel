#include "المتغيرات.h"
#include "القلب/الوحدات/محمل_التطبيقات/سياق_اللغة.h"
#include "../ادوات/ادوات.h"
#include "../طبقة_الترجمة/مخزن_المتغيرات.h"
#include "القلب/الوحدات/محرك_الفيديو/محرك_الفيديو.h"
#include "القلب/اختبار/مؤشر_الفأرة/مؤشر_الفأرة.h"
#include "../طبقة_التنفيذ/بوابة_التنفيذ.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"

static int is_builtin_variable(const char *name)
{
    if (!name) return 0;
    return (strcmp(name, "عرض_الشاشة")     == 0 ||   /* كان موجودًا */
            strcmp(name, "ارتفاع_الشاشة")  == 0 ||   /* كان موجودًا */
            strcmp(name, "خط_الشاشة")       == 0 ||   /* جديد */
            strcmp(name, "مؤشر_الخلفية")   == 0 ||   /* جديد */
            strcmp(name, "مؤشر_الأمامية")  == 0 ||   /* جديد */
            strcmp(name, "موضع_س")         == 0 ||
            strcmp(name, "موضع_ص")         == 0 ||
            strcmp(name, "حركة_س")         == 0 ||
            strcmp(name, "حركة_ص")         == 0 ||
            strcmp(name, "زر_الفأرة")      == 0 ||
            strcmp(name, "نقرة_الفأرة")    == 0 ||
            strcmp(name, "الوقت_ثانية")    == 0 ||
            strcmp(name, "الوقت_دقيقة")    == 0 ||
            strcmp(name, "الوقت_ساعة")     == 0 ||
            strcmp(name, "الوقت_يوم")      == 0 ||
            strcmp(name, "الوقت_شهر")      == 0 ||
            strcmp(name, "الوقت_سنة")      == 0);
}

static variable_t *find_variable(arp_context_t *ctx, const char *name)
{
    if (!ctx || !name) return NULL;
    for (int i = 0; i < ctx->variable_count; i++)
        if (ctx->variables[i].used &&
            ctx->variables[i].name &&
            strcmp(ctx->variables[i].name, name) == 0)
            return &ctx->variables[i];
    return NULL;
}

static variable_t *alloc_variable_slot(arp_context_t *ctx)
{
    if (!ctx) return NULL;
    if (ctx->variable_count >= ctx->variables_capacity)
    {
        int nc = ctx->variables_capacity ? ctx->variables_capacity * 2 : 16;
        variable_t *nv = (variable_t *)memory_api.alloc(nc * sizeof(variable_t));
        if (!nv) return NULL;
        if (ctx->variables)
        {
            memcpy(nv, ctx->variables, ctx->variable_count * sizeof(variable_t));
            memory_api.free(ctx->variables);
        }
        ctx->variables = nv;
        ctx->variables_capacity = nc;
    }
    variable_t *v = &ctx->variables[ctx->variable_count++];
    memset(v, 0, sizeof(variable_t));
    arp_value_init(&v->value, ARP_TYPE_UNKNOWN);
    return v;
}

void variables_init(void)
{
    arp_context_t *ctx = arp_get_current_context();
    if (!ctx) return;

    for (int i = 0; i < ctx->variable_count; i++)
    {
        if (ctx->variables[i].name) memory_api.free(ctx->variables[i].name);
        arp_value_free(&ctx->variables[i].value);
    }
    if (ctx->variables) memory_api.free(ctx->variables);
    ctx->variables = NULL;
    ctx->variable_count = 0;
    ctx->variables_capacity = 0;
}

int variable_declare(const char *name, arp_type_t type,
                     const arp_value_t *initial_value)
{
    if (!name || !*name || !initial_value) return -1;
    if (is_builtin_variable(name)) return -1;
    if (type == ARP_TYPE_UNKNOWN) return -1;

    arp_context_t *ctx = arp_get_current_context();
    if (!ctx) return -1;

    variable_t *v = find_variable(ctx, name);
    if (!v)
    {
        v = alloc_variable_slot(ctx);
        if (!v) return -1;
        v->name = arp_strdup(name);
        if (!v->name) return -1;
        v->used = 1;
    }

    if (!arp_value_is_assignable(type, initial_value->type))
        return -1;

    arp_value_t coerced;
    arp_value_init(&coerced, type);
    if (arp_value_coerce(initial_value, &coerced) < 0) return -1;

    arp_value_free(&v->value);
    v->value = coerced;
    v->declared_type = type;
    return 0;
}

int variable_get_declared_type(const char *name, arp_type_t *out_type)
{
    if (!name || !out_type) return -1;
    arp_context_t *ctx = arp_get_current_context();
    if (!ctx) return -1;
    variable_t *v = find_variable(ctx, name);
    if (!v) return -1;
    *out_type = v->declared_type;
    return 0;
}

int variable_get_value(const char *name, arp_value_t *out)
{
    if (!name || !out) return -1;

    /* 1) VM محلي */
    bytecode_vm_t *vm = arp_get_active_vm();
    if (vm && bytecode_vm_get_local(vm, name, out) == 0)
        return 0;

    /* ═══════════════════════════════════════════════════════════
     * معلومات الشاشة — تتيح الرسم المباشر على الذاكرة
     *
     *   عرض_الشاشة     → عدد البكسلات أفقيًّا
     *   ارتفاع_الشاشة  → عدد البكسلات رأسيًّا
     *   خط_الشاشة       → البايتات لكل صف (pitch × 4)
     *   مؤشر_الخلفية   → عنوان البافر الخلفي (بايت)
     *   مؤشر_الأمامية  → عنوان البافر الأمامي (بايت)
     *
     * مثال للرسم المباشر من ARP:
     *   ذاكرة_اكتب32(مؤشر_الخلفية +
     *                y * خط_الشاشة +
     *                x * 4,
     *                لون)
     * ═══════════════════════════════════════════════════════════ */

    if (strcmp(name, "عرض_الشاشة") == 0)
    {
        arp_value_make_int(out, (int64_t)fb_get_width());
        return 0;
    }

    if (strcmp(name, "ارتفاع_الشاشة") == 0)
    {
        arp_value_make_int(out, (int64_t)fb_get_height());
        return 0;
    }

    if (strcmp(name, "خط_الشاشة") == 0)
    {
        /* fb_get_pitch() يعيد عدد uint32_t في الصف، نضربه في 4
         * ليعطي البايتات كما يتوقّعها ذاكرة_اكتب32(). */
        arp_value_make_int(out, (int64_t)fb_get_pitch() * 4);
        return 0;
    }

    if (strcmp(name, "مؤشر_الخلفية") == 0)
    {
        arp_value_make_ptr(out,
            (uintptr_t)execute_get_back_buffer_ptr());
        return 0;
    }

    if (strcmp(name, "مؤشر_الأمامية") == 0)
    {
        arp_value_make_ptr(out,
            (uintptr_t)execute_get_front_buffer_ptr());
        return 0;
    }

    if (strcmp(name, "موضع_س") == 0)  { arp_value_make_int(out, (int64_t)mouse_x); return 0; }
    if (strcmp(name, "موضع_ص") == 0)  { arp_value_make_int(out, (int64_t)mouse_y); return 0; }
    if (strcmp(name, "حركة_س") == 0)  { arp_value_make_int(out, (int64_t)mouse_dx); return 0; }
    if (strcmp(name, "حركة_ص") == 0)  { arp_value_make_int(out, (int64_t)mouse_dy); return 0; }
    if (strcmp(name, "زر_الفأرة") == 0)   { arp_value_make_bool(out, mouse_left); return 0; }
    if (strcmp(name, "نقرة_الفأرة") == 0) { arp_value_make_bool(out, mouse_consume_click()); return 0; }

    if (strcmp(name, "الوقت_ثانية") == 0) { int v = 0; execute_rtc_get(0, &v); arp_value_make_int(out, v); return 0; }
    if (strcmp(name, "الوقت_دقيقة") == 0) { int v = 0; execute_rtc_get(1, &v); arp_value_make_int(out, v); return 0; }
    if (strcmp(name, "الوقت_ساعة")  == 0) { int v = 0; execute_rtc_get(2, &v); arp_value_make_int(out, v); return 0; }
    if (strcmp(name, "الوقت_يوم")   == 0) { int v = 0; execute_rtc_get(3, &v); arp_value_make_int(out, v); return 0; }
    if (strcmp(name, "الوقت_شهر")   == 0) { int v = 0; execute_rtc_get(4, &v); arp_value_make_int(out, v); return 0; }
    if (strcmp(name, "الوقت_سنة")   == 0) { int v = 0; execute_rtc_get(5, &v); arp_value_make_int(out, v); return 0; }

    /* 3) متغيرات عادية */
    arp_context_t *ctx = arp_get_current_context();
    if (!ctx) return -1;
    variable_t *v = find_variable(ctx, name);
    if (!v) return -1;
    return arp_value_copy(out, &v->value);
}

int variable_set_value(const char *name, const arp_value_t *value)
{
    if (!name || !*name || !value) return -1;
    if (is_builtin_variable(name)) return -1;

    arp_context_t *ctx = arp_get_current_context();
    if (!ctx) return -1;

    variable_t *v = find_variable(ctx, name);
    if (!v)
    {
        v = alloc_variable_slot(ctx);
        if (!v) return -1;
        v->name = arp_strdup(name);
        if (!v->name) return -1;
        v->used = 1;
    }

    if (v->declared_type != ARP_TYPE_UNKNOWN)
    {
        if (!arp_value_is_assignable(v->declared_type, value->type))
            return -1;

        arp_value_t coerced;
        arp_value_init(&coerced, v->declared_type);
        if (arp_value_coerce(value, &coerced) < 0) return -1;

        arp_value_free(&v->value);
        v->value = coerced;
    }
    else
    {
        arp_value_free(&v->value);
        if (arp_value_copy(&v->value, value) < 0) return -1;
    }

    return 0;
}

static void copy_to_buf(char *dst, int size, const char *src)
{
    if (!dst || size <= 0) return;
    if (!src) { dst[0] = '\0'; return; }
    int i = 0;
    while (src[i] && i < size - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

int variable_to_string(const char *name, char *buffer, int buffer_size)
{
    if (!name || !buffer || buffer_size <= 0) return -1;

    arp_value_t v;
    arp_value_init(&v, ARP_TYPE_UNKNOWN);
    if (variable_get_value(name, &v) < 0) return -1;

    char *s = NULL;
    int rc = arp_value_to_text(&v, &s);
    arp_value_free(&v);
    if (rc < 0) return -1;

    copy_to_buf(buffer, buffer_size, s);
    memory_api.free(s);
    return 0;
}
