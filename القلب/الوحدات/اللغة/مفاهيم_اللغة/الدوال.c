#include "الدوال.h"
#include "محرك_العمليات/محرك_العمليات.h"
#include "محرك_العمليات/بوابة_العمليات.h"
#include "القلب/الوحدات/محمل_التطبيقات/سياق_اللغة.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "رياضيات.h"
#include "../ادوات/ادوات.h"

void functions_init(void)
{
    arp_context_t *ctx = arp_get_current_context();
    if (!ctx) return;
    ctx->function_count = 0;
}

arp_function_t *function_find(const char *name)
{
    if (!name) return 0;
    arp_context_t *ctx = arp_get_current_context();
    if (!ctx) return 0;
    for (int i = 0; i < ctx->function_count; i++)
        if (ctx->functions[i].name &&
            strcmp(ctx->functions[i].name, name) == 0)
            return &ctx->functions[i];
    return 0;
}

void function_free(arp_function_t *f)
{
    if (!f) return;

    if (f->name) { memory_api.free(f->name); f->name = NULL; }

    if (f->parameters)
    {
        for (int i = 0; i < f->parameter_count; i++)
            if (f->parameters[i]) memory_api.free(f->parameters[i]);
        memory_api.free(f->parameters);
        f->parameters = NULL;
    }
    f->parameter_count = 0;

    if (f->lines)
    {
        for (int i = 0; i < f->line_count; i++)
            if (f->lines[i]) memory_api.free(f->lines[i]);
        memory_api.free(f->lines);
        f->lines = NULL;
    }
    f->line_count = 0;

    /* ===== تحرير كود JIT المُخزَّن (إن وُجد) ===== */
    if (f->jit_code)
    {
        memory_api.free(f->jit_code);
        f->jit_code = NULL;
        f->jit_code_size = 0;
    }
}

static int fill_function(
    arp_function_t *f,
    const char *name,
    const char **parameters,
    int parameter_count,
    const char **lines,
    int line_count
)
{
    memset(f, 0, sizeof(arp_function_t));

    f->name = arp_strdup(name);
    if (!f->name) return -1;

    if (parameter_count > 0)
    {
        f->parameters = (char**)memory_api.alloc(parameter_count * sizeof(char*));
        if (!f->parameters) { function_free(f); return -1; }
        f->parameter_count = parameter_count;

        for (int i = 0; i < parameter_count; i++)
        {
            f->parameters[i] = arp_strdup(parameters[i]);
            if (!f->parameters[i]) { function_free(f); return -1; }
        }
    }

    if (line_count > 0)
    {
        f->lines = (char**)memory_api.alloc(line_count * sizeof(char*));
        if (!f->lines) { function_free(f); return -1; }
        f->line_count = line_count;

        for (int i = 0; i < line_count; i++)
        {
            f->lines[i] = arp_strdup(lines[i]);
            if (!f->lines[i]) { function_free(f); return -1; }
        }
    }

    /* jit_code و jit_code_size يبقيان صفرًا حتى أول استدعاء */
    return 0;
}

int function_register(
    const char *name,
    const char **parameters,
    int parameter_count,
    const char **lines,
    int line_count
)
{
    if (!name) return -1;
    if (parameter_count < 0 || line_count < 0) return -1;
    if (parameter_count > 0 && !parameters) return -1;
    if (line_count > 0 && !lines) return -1;

    arp_context_t *ctx = arp_get_current_context();
    if (!ctx) return -1;

    arp_function_t *existing = function_find(name);
    if (existing)
    {
        /* function_free يُحرِّر jit_code القديم تلقائيًا */
        function_free(existing);
        return fill_function(existing, name, parameters,
                             parameter_count, lines, line_count);
    }

    if (ctx->function_count >= ctx->functions_capacity)
    {
        int new_cap = ctx->functions_capacity ? ctx->functions_capacity * 2 : 4;
        arp_function_t *na = (arp_function_t *)
            memory_api.alloc(new_cap * sizeof(arp_function_t));
        if (!na) return -1;
        if (ctx->functions)
        {
            memcpy(na, ctx->functions,
                   ctx->function_count * sizeof(arp_function_t));
            memory_api.free(ctx->functions);
        }
        ctx->functions = na;
        ctx->functions_capacity = new_cap;
    }

    arp_function_t *f = &ctx->functions[ctx->function_count];
    if (fill_function(f, name, parameters, parameter_count, lines, line_count) < 0)
        return -1;
    ctx->function_count++;
    return 0;
}

int function_register_aot(
    const char *name,
    const char **parameters,
    int parameter_count,
    void *jit_code,
    size_t jit_code_size
)
{
    if (!name || !jit_code) return -1;
    
    arp_context_t *ctx = arp_get_current_context();
    if (!ctx) return -1;
    
    /* ابحث عن الدالة موجودة */
    arp_function_t *existing = function_find(name);
    if (existing) {
        function_free(existing);
    } else {
        /* أضف مساحة */
        if (ctx->function_count >= ctx->functions_capacity) {
            int new_cap = ctx->functions_capacity ? ctx->functions_capacity * 2 : 4;
            arp_function_t *na = (arp_function_t *)
                memory_api.alloc(new_cap * sizeof(arp_function_t));
            if (!na) return -1;
            if (ctx->functions) {
                memcpy(na, ctx->functions,
                       ctx->function_count * sizeof(arp_function_t));
                memory_api.free(ctx->functions);
            }
            ctx->functions = na;
            ctx->functions_capacity = new_cap;
        }
        existing = &ctx->functions[ctx->function_count++];
    }
    
    memset(existing, 0, sizeof(arp_function_t));
    existing->name = arp_strdup(name);
    if (!existing->name) return -1;
    
    if (parameter_count > 0 && parameters) {
        existing->parameters = (char **)memory_api.alloc(parameter_count * sizeof(char *));
        if (!existing->parameters) { function_free(existing); return -1; }
        existing->parameter_count = parameter_count;
        for (int i = 0; i < parameter_count; i++) {
            existing->parameters[i] = arp_strdup(parameters[i]);
            if (!existing->parameters[i]) { function_free(existing); return -1; }
        }
    }
    
    existing->jit_code = jit_code;
    existing->jit_code_size = jit_code_size;
    existing->lines = NULL;
    existing->line_count = 0;
    
    return 0;
}
