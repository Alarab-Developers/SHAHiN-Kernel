#include "الدوال.h"
#include "محرك_العمليات/محرك_العمليات.h"
#include "محرك_العمليات/بوابة_العمليات.h"
#include "القلب/الوحدات/محمل_التطبيقات/سياق_اللغة.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"

void functions_init(void)
{
    arp_context_t* ctx = arp_get_current_context();
    if (!ctx) return;
    ctx->function_count = 0;
    // لا نحرر المصفوفة هنا لأنها قد تحتوي على دوال من مكتبات مضافة
}

arp_function_t* function_find(const char* name)
{
    if (!name) return 0;
    arp_context_t* ctx = arp_get_current_context();
    if (!ctx) return 0;
    for (int i = 0; i < ctx->function_count; i++)
    {
        if (strcmp(ctx->functions[i].name, name) == 0)
            return &ctx->functions[i];
    }
    return 0;
}

// يملأ f بالاسم/المعاملات/الأسطر. يفترض أن f فارغ تماماً
// (parameters=NULL, lines=NULL) عند الدخول.
static int fill_function(
    arp_function_t *f,
    const char *name,
    const char **parameters,
    int parameter_count,
    const char **lines,
    int line_count
)
{
    strcpy(f->name, name);

    // تخصيص المعاملات فقط إذا كان هناك معاملات
    if (parameter_count > 0)
    {
        f->parameters = (char**)memory_api.alloc(parameter_count * sizeof(char*));
        if (!f->parameters) return -1;
        f->parameter_count = parameter_count;
        for (int i = 0; i < parameter_count; i++)
        {
            size_t len = strlen(parameters[i]) + 1;
            f->parameters[i] = (char*)memory_api.alloc(len);
            if (!f->parameters[i])
            {
                for (int j = 0; j < i; j++) memory_api.free(f->parameters[j]);
                memory_api.free(f->parameters);
                f->parameters = NULL;
                f->parameter_count = 0;
                return -1;
            }
            strcpy(f->parameters[i], parameters[i]);
        }
    }
    else
    {
        f->parameters = NULL;
        f->parameter_count = 0;
    }

    // تخصيص الأسطر
    f->lines = (char**)memory_api.alloc(line_count * sizeof(char*));
    if (!f->lines)
    {
        if (f->parameters)
        {
            for (int i = 0; i < f->parameter_count; i++) memory_api.free(f->parameters[i]);
            memory_api.free(f->parameters);
            f->parameters = NULL;
            f->parameter_count = 0;
        }
        return -1;
    }
    f->line_count = line_count;
    for (int i = 0; i < line_count; i++)
    {
        size_t len = strlen(lines[i]) + 1;
        f->lines[i] = (char*)memory_api.alloc(len);
        if (!f->lines[i])
        {
            for (int j = 0; j < i; j++) memory_api.free(f->lines[j]);
            memory_api.free(f->lines);
            f->lines = NULL;
            f->line_count = 0;
            if (f->parameters)
            {
                for (int j = 0; j < f->parameter_count; j++) memory_api.free(f->parameters[j]);
                memory_api.free(f->parameters);
                f->parameters = NULL;
                f->parameter_count = 0;
            }
            return -1;
        }
        strcpy(f->lines[i], lines[i]);
    }

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
    if (!name || !lines) return -1;                  // name و lines إجبارية
    if (parameter_count < 0 || line_count < 0) return -1;
    if (parameter_count > 0 && !parameters) return -1; // إذا كان هناك معاملات، يجب أن تكون parameters != NULL

    arp_context_t* ctx = arp_get_current_context();
    if (!ctx) return -1;

    // إعادة تعريف دالة موجودة: نحرر جسمها ومعاملاتها القديمة
    // ونعيد استخدام نفس الفتحة بدلاً من رفض التعريف الجديد
    arp_function_t* existing = function_find(name);
    if (existing)
    {
        if (existing->parameters)
        {
            for (int i = 0; i < existing->parameter_count; i++) memory_api.free(existing->parameters[i]);
            memory_api.free(existing->parameters);
        }
        if (existing->lines)
        {
            for (int i = 0; i < existing->line_count; i++) memory_api.free(existing->lines[i]);
            memory_api.free(existing->lines);
        }
        existing->parameters = NULL;
        existing->parameter_count = 0;
        existing->lines = NULL;
        existing->line_count = 0;

        return fill_function(existing, name, parameters, parameter_count, lines, line_count);
    }

    // توسيع مصفوفة الدوال إذا لزم الأمر
    if (ctx->function_count >= ctx->functions_capacity)
    {
        int new_capacity = (ctx->functions_capacity == 0) ? 4 : ctx->functions_capacity * 2;
        size_t new_size = new_capacity * sizeof(arp_function_t);
        arp_function_t* new_array = (arp_function_t*)memory_api.alloc(new_size);
        if (!new_array) return -1;
        if (ctx->functions)
        {
            size_t old_size = ctx->function_count * sizeof(arp_function_t);
            memcpy(new_array, ctx->functions, old_size);
            memory_api.free(ctx->functions);
        }
        ctx->functions = new_array;
        ctx->functions_capacity = new_capacity;
    }

    arp_function_t* f = &ctx->functions[ctx->function_count];

    if (fill_function(f, name, parameters, parameter_count, lines, line_count) < 0)
        return -1;

    ctx->function_count++;
    return 0;
}
