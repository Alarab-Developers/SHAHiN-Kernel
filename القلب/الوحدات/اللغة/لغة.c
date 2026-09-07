#include "لغة.h"
#include "ادوات/ادوات.h"
#include "المفكر/المحلل.h"
#include "المفكر/مفاهيم_اللغة/الدوال.h"
#include "المفكر/مفاهيم_اللغة/المتغيرات.h"
#include "المفكر/مفاهيم_اللغة/التعليقات.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "القلب/الوحدات/شاشة_التطوير/شاشة_التطوير.h"

typedef struct
{
    char **lines;
    int line_count;
} arp_body_context_t;

static int run_line(const char *line)
{
    return arp_execute_line(line);
}

/* ================================================================ */
/* استخراج اسم الدالة والمعاملات (ديناميكياً)                       */
/* ================================================================ */
static int parse_function_definition(
    const char *line,
    char *name,
    char ***parameters_out,
    int *parameter_count
)
{
    if (!line || !name || !parameters_out || !parameter_count) return -1;

    const char *p = skip_spaces(line);
    if (!starts_with(p, "دالة(")) return -1;
    p = skip_prefix(p, "دالة(");
    p = skip_spaces(p);

    // قراءة الاسم
    int name_length = 0;
    while (*p && *p != ',' && *p != ')' && name_length < 63)
        name[name_length++] = *p++;
    while (name_length > 0 && (name[name_length-1] == ' ' || name[name_length-1] == '\t'))
        name_length--;
    name[name_length] = '\0';
    if (name_length == 0) return -1;

    // قراءة المعاملات
    char **params = NULL;
    int count = 0;

    if (*p == ')')
    {
        p++;
        p = skip_spaces(p);
        if (*p != '\0')
        {
            if (params) { for (int i=0; i<count; i++) memory_api.free(params[i]); memory_api.free(params); }
            return -1;
        }
        *parameters_out = NULL;
        *parameter_count = 0;
        return 0;
    }

    if (*p != ',') return -1;

    while (*p)
    {
        if (*p == ',') p++;
        p = skip_spaces(p);
        if (*p == ')')
        {
            p++;
            p = skip_spaces(p);
            if (*p != '\0')
            {
                if (params) { for (int i=0; i<count; i++) memory_api.free(params[i]); memory_api.free(params); }
                return -1;
            }
            break;
        }

        // توسيع مصفوفة المعاملات
        char **new_params = (char**)memory_api.alloc((count + 1) * sizeof(char*));
        if (!new_params)
        {
            if (params) { for (int i=0; i<count; i++) memory_api.free(params[i]); memory_api.free(params); }
            return -1;
        }
        for (int i = 0; i < count; i++) new_params[i] = params[i];
        if (params) memory_api.free(params);
        params = new_params;

        // قراءة اسم المعامل
        int length = 0;
        char temp[64];
        while (*p && *p != ',' && *p != ')' && length < 63)
            temp[length++] = *p++;
        temp[length] = '\0';
        while (length > 0 && (temp[length-1] == ' ' || temp[length-1] == '\t'))
            length--;
        temp[length] = '\0';
        if (length == 0)
        {
            for (int i = 0; i < count; i++) memory_api.free(params[i]);
            memory_api.free(params);
            return -1;
        }

        params[count] = (char*)memory_api.alloc(length + 1);
        if (!params[count])
        {
            for (int i = 0; i < count; i++) memory_api.free(params[i]);
            memory_api.free(params);
            return -1;
        }
        strcpy(params[count], temp);
        count++;

        p = skip_spaces(p);
        if (*p == ',')
        {
            p++;
            p = skip_spaces(p);
            continue;
        }
        if (*p == ')')
        {
            p++;
            p = skip_spaces(p);
            if (*p != '\0')
            {
                for (int i = 0; i < count; i++) memory_api.free(params[i]);
                memory_api.free(params);
                return -1;
            }
            break;
        }
        // خطأ
        for (int i = 0; i < count; i++) memory_api.free(params[i]);
        memory_api.free(params);
        return -1;
    }

    *parameters_out = params;
    *parameter_count = count;
    return 0;
}

/* ================================================================ */
/* تشغيل السكربت                                                     */
/* ================================================================ */
static int execute_script(const char *script, uint32_t size)
{
    // رسائل التصحيح
    if (!script || size == 0)
    {
        // development_screen_println("ARP: script empty or NULL");
        // development_screen_update();
        return -1;
    }

    // إزالة التعليقات
    char *cleaned = NULL;
    uint32_t cleaned_size = 0;
    if (comment_remove(script, size, &cleaned, &cleaned_size) < 0)
    {
        // development_screen_println("ARP: comment_remove FAILED");
        // development_screen_update();
        return -1;
    }

    // استخدام النص المُنظّف
    const char *exec_script = cleaned ? cleaned : script;
    uint32_t exec_size = cleaned ? cleaned_size : size;

    // development_screen_println("ARP: script size =");
    // development_screen_print_hex64(exec_size);
    // development_screen_println("");
    // development_screen_update();

    uint32_t offset = 0;
    while (offset < exec_size)
    {
        char *line;
        uint32_t line_len;
        int ret = read_line(exec_script, exec_size, &offset, &line, &line_len);

        if (ret < 0)
        {
            // development_screen_println("ARP: read_line FAILED at offset");
            // development_screen_print_hex64(offset);
            // development_screen_update();
            if (cleaned) memory_api.free(cleaned);
            return -1;
        }

        if (!line || line_len == 0)
        {
            if (line) memory_api.free(line);
            // development_screen_println("ARP: empty line at offset");
            // development_screen_print_hex64(offset);
            // development_screen_update();
            continue;
        }

        // development_screen_println("ARP: line =");
        // development_screen_print(line);
        // development_screen_println("");
        // development_screen_update();

        const char *p = skip_spaces(line);
        if (*p == '\0')
        {
            // development_screen_println("ARP: whitespace line");
            // development_screen_update();
            memory_api.free(line);
            continue;
        }

        /* ======================================================== */
        /* تعريف دالة                                                */
        /* ======================================================== */
        if (starts_with(p, "دالة("))
        {
            // development_screen_println("ARP: function definition detected");
            // development_screen_update();

            char name[64];
            char **params = NULL;
            int param_count = 0;

            if (parse_function_definition(p, name, &params, &param_count) < 0)
            {
                // development_screen_println("ARP: parse_function_definition FAILED");
                // development_screen_update();
                memory_api.free(line);
                if (cleaned) memory_api.free(cleaned);
                return -1;
            }

            // development_screen_println("ARP: function name =");
            // development_screen_print(name);
            // development_screen_println("");
            // development_screen_println("ARP: param count =");
            // development_screen_print_hex64(param_count);
            // development_screen_println("");
            // development_screen_update();

            arp_body_context_t* body_ctx = (arp_body_context_t*)memory_api.alloc(sizeof(arp_body_context_t));
            if (!body_ctx)
            {
                if (params) { for (int i=0; i<param_count; i++) memory_api.free(params[i]); memory_api.free(params); }
                memory_api.free(line);
                if (cleaned) memory_api.free(cleaned);
                return -1;
            }
            body_ctx->lines = NULL;
            body_ctx->line_count = 0;

            int found_end = 0;

            while (offset < exec_size)
            {
                char *body_line;
                uint32_t body_len;
                if (read_line(exec_script, exec_size, &offset, &body_line, &body_len) < 0)
                {
                    if (body_ctx->lines) { for (int i=0; i<body_ctx->line_count; i++) memory_api.free(body_ctx->lines[i]); memory_api.free(body_ctx->lines); }
                    memory_api.free(body_ctx);
                    memory_api.free(line);
                    if (params) { for (int i=0; i<param_count; i++) memory_api.free(params[i]); memory_api.free(params); }
                    if (cleaned) memory_api.free(cleaned);
                    return -1;
                }
                if (!body_line || body_len == 0)
                {
                    if (body_line) memory_api.free(body_line);
                    continue;
                }

                const char *b = skip_spaces(body_line);
                if (*b == '\0')
                {
                    memory_api.free(body_line);
                    continue;
                }

                if (starts_with(b, "نهاية("))
                {
                    char end_name[64];
                    const char *e = skip_prefix(b, "نهاية(");
                    e = skip_spaces(e);
                    int end_len = 0;
                    while (*e && *e != ')' && end_len < 63)
                        end_name[end_len++] = *e++;
                    end_name[end_len] = '\0';
                    if (*e != ')')
                    {
                        memory_api.free(body_line);
                        if (body_ctx->lines) { for (int i=0; i<body_ctx->line_count; i++) memory_api.free(body_ctx->lines[i]); memory_api.free(body_ctx->lines); }
                        memory_api.free(body_ctx);
                        memory_api.free(line);
                        if (params) { for (int i=0; i<param_count; i++) memory_api.free(params[i]); memory_api.free(params); }
                        if (cleaned) memory_api.free(cleaned);
                        return -1;
                    }
                    e++;
                    e = skip_spaces(e);
                    if (*e != '\0' || strcmp(name, end_name) != 0)
                    {
                        memory_api.free(body_line);
                        if (body_ctx->lines) { for (int i=0; i<body_ctx->line_count; i++) memory_api.free(body_ctx->lines[i]); memory_api.free(body_ctx->lines); }
                        memory_api.free(body_ctx);
                        memory_api.free(line);
                        if (params) { for (int i=0; i<param_count; i++) memory_api.free(params[i]); memory_api.free(params); }
                        if (cleaned) memory_api.free(cleaned);
                        return -1;
                    }
                    memory_api.free(body_line);
                    found_end = 1;
                    break;
                }

                // إضافة السطر إلى body_ctx
                char **new_lines = (char**)memory_api.alloc((body_ctx->line_count + 1) * sizeof(char*));
                if (!new_lines)
                {
                    for (int i = 0; i < body_ctx->line_count; i++) memory_api.free(body_ctx->lines[i]);
                    if (body_ctx->lines) memory_api.free(body_ctx->lines);
                    memory_api.free(body_ctx);
                    memory_api.free(body_line);
                    memory_api.free(line);
                    if (params) { for (int i=0; i<param_count; i++) memory_api.free(params[i]); memory_api.free(params); }
                    if (cleaned) memory_api.free(cleaned);
                    return -1;
                }
                for (int i = 0; i < body_ctx->line_count; i++)
                    new_lines[i] = body_ctx->lines[i];
                if (body_ctx->lines) memory_api.free(body_ctx->lines);
                body_ctx->lines = new_lines;
                body_ctx->lines[body_ctx->line_count] = body_line;
                body_ctx->line_count++;
            }

            if (!found_end)
            {
                for (int i = 0; i < body_ctx->line_count; i++) memory_api.free(body_ctx->lines[i]);
                if (body_ctx->lines) memory_api.free(body_ctx->lines);
                memory_api.free(body_ctx);
                memory_api.free(line);
                if (params) { for (int i=0; i<param_count; i++) memory_api.free(params[i]); memory_api.free(params); }
                if (cleaned) memory_api.free(cleaned);
                return -1;
            }

            // تسجيل الدالة
            // development_screen_println("ARP: registering function...");
            // development_screen_update();

            if (function_register(name, (const char**)params, param_count,
                                  (const char**)body_ctx->lines, body_ctx->line_count) < 0)
            {
                // development_screen_println("ARP: function_register FAILED");
                // development_screen_update();
                for (int i = 0; i < body_ctx->line_count; i++) memory_api.free(body_ctx->lines[i]);
                if (body_ctx->lines) memory_api.free(body_ctx->lines);
                memory_api.free(body_ctx);
                memory_api.free(line);
                if (params) { for (int i=0; i<param_count; i++) memory_api.free(params[i]); memory_api.free(params); }
                if (cleaned) memory_api.free(cleaned);
                return -1;
            }

            // تحرير body_ctx (الأسطر نُسخت داخل function_register)
            for (int i = 0; i < body_ctx->line_count; i++) memory_api.free(body_ctx->lines[i]);
            if (body_ctx->lines) memory_api.free(body_ctx->lines);
            memory_api.free(body_ctx);

            // تحرير المعاملات المؤقتة
            if (params) { for (int i=0; i<param_count; i++) memory_api.free(params[i]); memory_api.free(params); }

            memory_api.free(line);
            // development_screen_println("ARP: function registered");
            // development_screen_update();
            continue;
        }

        /* ======================================================== */
        /* سطر عادي                                                  */
        /* ======================================================== */
        // development_screen_println("ARP: executing normal line");
        // development_screen_update();

        if (arp_execute_line(line) < 0)
        {
            // development_screen_println("ARP: arp_execute_line FAILED");
            // development_screen_update();
            memory_api.free(line);
            if (cleaned) memory_api.free(cleaned);
            return -1;
        }
        memory_api.free(line);
    }

    // تحرير cleaned
    if (cleaned) memory_api.free(cleaned);

    // development_screen_println("ARP: script execution finished");
    // development_screen_update();
    return 0;
}

static int run_script(const char *script, uint32_t size)
{
    // development_screen_println("ARP: init context");
    // development_screen_update();
    functions_init();
    variables_init();
    // development_screen_println("ARP: context initialized");
    // development_screen_update();
    return execute_script(script, size);
}

int arp_run_library(const char *script, uint32_t size)
{
    return execute_script(script, size);
}

arp_api_t arp_api =
{
    .run_line   = run_line,
    .run_script = run_script
};
