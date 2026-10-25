#include "لغة.h"
#include "ادوات/ادوات.h"
#include "مفاهيم_اللغة/الدوال.h"
#include "مفاهيم_اللغة/الهياكل.h"
#include "مفاهيم_اللغة/المتغيرات.h"
#include "مفاهيم_اللغة/التعليقات.h"
#include "مفاهيم_اللغة/رياضيات.h"
#include "مفاهيم_اللغة/مكتبات_اللغة.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "طبقة_الترجمة/مجمع_الترجمة.h"
#include "طبقة_الترجمة/مترجم_اللغة.h"
#include "طبقة_الترجمة/التصريح_بالأنواع.h"
#include "طبقة_الترجمة/المحلل.h"

#include "القلب/الوحدات/محمل_التطبيقات/سياق_اللغة.h"
#include "القلب/الوحدات/محمل_التطبيقات/قيم_الملف.h"


/* ================================================================ */
/* أوضاع تشغيل السكربت — AOT خالص                                   */
/* ================================================================ */
#define ARP_MODE_COMPILE  0

/* ================================================================ */
/* دالة مساعدة: البحث عن ملف في الأرشيف (مُعلَنة في قيم_الملف.h)   */
/* ================================================================ */
extern file_t *find_file(const char *name);

/* ================================================================ */
/* تحرير مصفوفة أسطر ديناميكية                                      */
/* ================================================================ */
static void free_lines_array_helper(char **lines, int count)
{
    if (!lines) return;
    for (int i = 0; i < count; i++) memory_api.free(lines[i]);
    memory_api.free(lines);
}

/* ================================================================ */
/* ★★★ دعم الأسطر المتعددة والأوامر المتعددة في سطر واحد ★★★       */
/* ================================================================ */

/* عدّ الأقواس والأقواس المعقوفة غير المغلقة في سطر (يتجاهل ما بين " ")
 *
 * ★ الإصلاح: نحسب أيضًا '{' و '}' لأن المصفوفات الحرفية
 *   ({1,2,3} و {{...},{...}}) قد تمتد على عدة أسطر.
 */
static int count_unclosed_parens(const char *line)
{
    int depth = 0, in_str = 0;
    const char *p = line;
    while (*p)
    {
        if (*p == '"') { in_str = !in_str; }
        else if (!in_str)
        {
            if (*p == '(' || *p == '{') depth++;
            else if (*p == ')' || *p == '}') { if (depth > 0) depth--; }
        }
        p++;
    }
    return depth;
}

/* هل السطر ناقص؟ (لم تُغلق كل الأقواس) */
static int line_is_incomplete(const char *line)
{
    return count_unclosed_parens(line) > 0;
}

/* دمج الأسطر الناقصة مع التالية */
static int join_incomplete_lines(char ***lines_ptr, int *count_ptr)
{
    char **lines = *lines_ptr;
    int total    = *count_ptr;
    int out_count = 0;

    char **out = (char **)memory_api.alloc(total * sizeof(char *));
    if (!out) return -1;

    int i = 0;
    while (i < total)
    {
        char *accum = lines[i];
        int needs_join = line_is_incomplete(accum);

        while (needs_join && i + 1 < total)
        {
            i++;
            char *next = lines[i];
            size_t la = strlen(accum);
            size_t lb = strlen(next);
            char *merged = (char *)memory_api.alloc(la + 1 + lb + 1);
            if (!merged)
            {
                memory_api.free(accum);
                memory_api.free(next);
                for (int k = 0; k < out_count; k++) memory_api.free(out[k]);
                memory_api.free(out);
                return -1;
            }
            memcpy(merged, accum, la);
            merged[la] = ' ';
            memcpy(merged + la + 1, next, lb + 1);

            memory_api.free(accum);
            memory_api.free(next);
            accum = merged;
            needs_join = line_is_incomplete(accum);
        }

        out[out_count++] = accum;
        i++;
    }

    if (lines) memory_api.free(lines);

    *lines_ptr = out;
    *count_ptr = out_count;
    return 0;
}

/* الكلمات المفتاحية بدون أقواس (عبارة كاملة بذاتها) */
static const char *const g_bare_keywords[] = {
    "والا", "نهاية_الشرط", "نهاية_الحلقة", "نهاية_الهيكل",
    NULL
};

/* هل q يبدأ بكلمة مفتاحية بدون أقواس؟ (وتنتهي بمسافة أو نهاية) */
static const char *match_bare_keyword(const char *q)
{
    for (int i = 0; g_bare_keywords[i]; i++)
    {
        const char *kw = g_bare_keywords[i];
        size_t len = strlen(kw);
        if (strncmp(q, kw, len) == 0)
        {
            char after = q[len];
            if (after == '\0' || after == ' ' || after == '\t')
                return kw;
        }
    }
    return NULL;
}

/*
 * find_stmt_end — يُعيد طول العبارة الأولى في p
 * (أو 0 إذا لم يجد عبارة).
 *
 * ★ القاعدة:
 *   بعد ')' مغلقة (depth=0)، إذا كان ما يلي ليس عملية حسابية
 *   ولا فاصلة ولا ')' → فهو بداية عبارة جديدة.
 *
 *   العمليات الحسابية: + - * / = < > ! & |
 *   الفواصل: , )
 *
 * هذا يسمح بتقسيم:
 *   اذا(س < 0) املأ_مستطيل(0,0,100,100,0) نهاية_الشرط
 * إلى ثلاثة أسطر، دون الحاجة لمعرفة كل الدوال المدمجة.
 */
static int find_stmt_end(const char *p)
{
    const char *start = p;

    /* حالة 1: كلمة مفتاحية بدون أقواس في البداية */
    const char *bare = match_bare_keyword(p);
    if (bare) return (int)strlen(bare);

    /* حالة 2: عبارة تبدأ بـ keyword( — ابحث عن ')' المطابقة */
    int depth = 0, in_str = 0;
    int seen_open = 0;

    while (*p)
    {
        if (*p == '"') { in_str = !in_str; p++; continue; }
        if (in_str) { p++; continue; }
        if (*p == '(') { depth++; seen_open = 1; p++; continue; }
        if (*p == ')')
        {
            depth--;
            p++;
            if (depth == 0 && seen_open)
            {
                /* انتهت العبارة. هل يبدأ شيء جديد بعدها؟ */
                const char *q = p;
                while (*q == ' ' || *q == '\t') q++;

                if (*q == '\0') return (int)(p - start);

                /* ★★★ القاعدة الجديدة ★★★ */
                char c = *q;
                int is_operator =
                    (c == '+' || c == '-' || c == '*' || c == '/' ||
                     c == '%' || c == '^' ||
                     c == '=' || c == '<' || c == '>' ||
                     c == ',' || c == ')' ||
                     c == '!' || c == '&' || c == '|' ||
                     c == '(');          /* ★ أبعاد متعددة: (3)(4) */

                if (!is_operator)
                    return (int)(p - start);   /* نقسم هنا */

                /* وإلا نكمل (عبارة مركبة) */
                p = q;
                seen_open = 0;
                continue;
            }
            continue;
        }
        p++;
    }

    /* لا توجد أقواس مغلقة → نأخذ بقية السطر */
    return (int)(p - start);
}

/* تقسيم سطر واحد إلى عدة عبارات */
static int split_line_into_statements(const char *line, char ***out_arr, int *out_count)
{
    int capacity = 4;
    char **arr = (char **)memory_api.alloc(capacity * sizeof(char *));
    if (!arr) return -1;
    int count = 0;

    const char *p = line;

    while (*p)
    {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        int len = find_stmt_end(p);
        if (len <= 0) break;

        char *stmt = arp_strndup(p, len);
        if (!stmt) goto fail;

        if (count >= capacity)
        {
            capacity *= 2;
            char **na = (char **)memory_api.alloc(capacity * sizeof(char *));
            if (!na) { memory_api.free(stmt); goto fail; }
            for (int i = 0; i < count; i++) na[i] = arr[i];
            memory_api.free(arr);
            arr = na;
        }
        arr[count++] = stmt;
        p += len;
    }

    *out_arr   = arr;
    *out_count = count;
    return 0;

fail:
    for (int i = 0; i < count; i++) memory_api.free(arr[i]);
    memory_api.free(arr);
    return -1;
}

/* معالجة مسبقة: دمج ثم تقسيم */
static int preprocess_script(char ***lines_ptr, int *count_ptr, int *cap_ptr)
{
    /* 1) دمج الأسطر الناقصة */
    if (join_incomplete_lines(lines_ptr, count_ptr) < 0) return -1;
    *cap_ptr = *count_ptr;

    /* 2) تقسيم الأسطر المتعددة */
    char **lines = *lines_ptr;
    int total    = *count_ptr;

    int out_cap = 16;
    char **out = (char **)memory_api.alloc(out_cap * sizeof(char *));
    if (!out) return -1;
    int out_count = 0;

    for (int i = 0; i < total; i++)
    {
        char **stmts = NULL;
        int stmt_count = 0;
        if (split_line_into_statements(lines[i], &stmts, &stmt_count) < 0)
        {
            memory_api.free(lines[i]);
            for (int k = i + 1; k < total; k++) memory_api.free(lines[k]);
            memory_api.free(lines);
            for (int j = 0; j < out_count; j++) memory_api.free(out[j]);
            memory_api.free(out);
            return -1;
        }
        memory_api.free(lines[i]);

        for (int k = 0; k < stmt_count; k++)
        {
            if (out_count >= out_cap)
            {
                out_cap *= 2;
                char **na = (char **)memory_api.alloc(out_cap * sizeof(char *));
                if (!na)
                {
                    memory_api.free(stmts[k]);
                    for (int m = k + 1; m < stmt_count; m++) memory_api.free(stmts[m]);
                    memory_api.free(stmts);
                    for (int j = 0; j < out_count; j++) memory_api.free(out[j]);
                    memory_api.free(out);
                    return -1;
                }
                for (int j = 0; j < out_count; j++) na[j] = out[j];
                memory_api.free(out);
                out = na;
            }
            out[out_count++] = stmts[k];
        }
        if (stmts) memory_api.free(stmts);
    }

    memory_api.free(lines);

    *lines_ptr = out;
    *count_ptr = out_count;
    *cap_ptr   = out_cap;
    return 0;
}

/* ================================================================ */
/* تضمين مكتبات ARP — مثل #include                                  */
/* ================================================================ */
#define ARP_MAX_INLINE_LIBS  64
#define ARP_MAX_INLINE_PASS  32

typedef struct {
    char *path;
} included_lib_t;

static int is_included(const included_lib_t *inc, int count, const char *path)
{
    for (int i = 0; i < count; i++)
        if (inc[i].path && strcmp(inc[i].path, path) == 0) return 1;
    return 0;
}

static int inline_libraries(char ***lines_ptr, int *count_ptr, int *cap_ptr)
{
    char **lines = *lines_ptr;
    int total    = *count_ptr;
    int cap      = *cap_ptr;

    included_lib_t included[ARP_MAX_INLINE_LIBS];
    int included_count = 0;
    memset(included, 0, sizeof(included));

    int changed = 1;
    int pass    = 0;

    while (changed && pass < ARP_MAX_INLINE_PASS)
    {
        changed = 0;
        pass++;

        int i = 0;
        while (i < total)
        {
            const char *p = skip_spaces(lines[i]);
            if (!starts_with(p, "تضمين(")) { i++; continue; }

            const char *open  = p + strlen("تضمين");
            const char *close = find_char(open, ')');
            if (!close) { i++; continue; }
            if (*skip_spaces(close + 1) != '\0') { i++; continue; }

            const char *s = open + 1;
            while (s < close && (*s == ' ' || *s == '\t')) s++;
            const char *e = close;
            while (e > s && (e[-1] == ' ' || e[-1] == '\t')) e--;

            int plen = e - s;
            if (plen < 3) { i++; continue; }
            if (*s != '"' || e[-1] != '"') { i++; continue; }

            char *path = arp_strndup(s + 1, plen - 2);
            if (!path) goto fail;

            /* ★ طبّع المسار إلى جذر الأرشيف (يحذف "./" و ".\" ويوحّد الشرطات) */
            char norm_path[256];
            arp_normalize_archive_path(path, norm_path, sizeof(norm_path));
            memory_api.free(path);

            /* تحقق من التضمين المسبق بالمسار المطبّع */
            if (is_included(included, included_count, norm_path))
            {
                memory_api.free(lines[i]);
                for (int k = i; k < total - 1; k++) lines[k] = lines[k + 1];
                total--;
                changed = 1;
                continue;
            }

            file_t *f = find_file(norm_path);   /* ★ المسار المطبّع */
            if (!f || !f->data || f->size == 0)
            {
                goto fail;
            }

            if (included_count >= ARP_MAX_INLINE_LIBS)
            {
                goto fail;
            }
            included[included_count++].path = arp_strdup(norm_path);

            char *cleaned = NULL;
            uint32_t cleaned_size = 0;
            if (comment_remove((const char *)f->data, f->size,
                               &cleaned, &cleaned_size) < 0)
            {
                goto fail;
            }

            const char *src      = cleaned ? cleaned : (const char *)f->data;
            uint32_t    src_size = cleaned ? cleaned_size : f->size;

            char **lib_lines = NULL;
            int    lib_count = 0;
            int    lib_cap   = 0;
            uint32_t off     = 0;

            while (off < src_size)
            {
                char *l; uint32_t ll;
                if (read_line(src, src_size, &off, &l, &ll) < 0)
                {
                    free_lines_array_helper(lib_lines, lib_count);
                    if (cleaned) memory_api.free(cleaned);
                    goto fail;
                }
                if (!l || ll == 0) { if (l) memory_api.free(l); continue; }

                if (lib_count >= lib_cap)
                {
                    int nc = lib_cap ? lib_cap * 2 : 16;
                    char **na = (char **)memory_api.alloc(nc * sizeof(char *));
                    if (!na)
                    {
                        memory_api.free(l);
                        free_lines_array_helper(lib_lines, lib_count);
                        if (cleaned) memory_api.free(cleaned);
                        goto fail;
                    }
                    for (int k = 0; k < lib_count; k++) na[k] = lib_lines[k];
                    if (lib_lines) memory_api.free(lib_lines);
                    lib_lines = na;
                    lib_cap   = nc;
                }
                lib_lines[lib_count++] = l;
            }

            if (cleaned) memory_api.free(cleaned);

            memory_api.free(lines[i]);

            int new_total = total - 1 + lib_count;

            if (new_total > cap)
            {
                int nc = cap ? cap : 16;
                while (nc < new_total) nc *= 2;

                char **na = (char **)memory_api.alloc(nc * sizeof(char *));
                if (!na)
                {
                    free_lines_array_helper(lib_lines, lib_count);
                    goto fail;
                }

                for (int k = 0; k < i; k++)         na[k] = lines[k];
                for (int k = 0; k < lib_count; k++) na[i + k] = lib_lines[k];
                for (int k = i + 1; k < total; k++) na[k - 1 + lib_count] = lines[k];

                memory_api.free(lines);
                lines = na;
                cap   = nc;
            }
            else
            {
                int shift = lib_count - 1;
                for (int k = total - 1; k > i; k--)
                    lines[k + shift] = lines[k];
                for (int k = 0; k < lib_count; k++)
                    lines[i + k] = lib_lines[k];
            }

            memory_api.free(lib_lines);

            total   = new_total;
            changed = 1;
        }
    }

    for (int k = 0; k < included_count; k++)
        if (included[k].path) memory_api.free(included[k].path);

    *lines_ptr = lines;
    *count_ptr = total;
    *cap_ptr   = cap;
    return 0;

fail:
    for (int k = 0; k < included_count; k++)
        if (included[k].path) memory_api.free(included[k].path);
    return -1;
}

/* ================================================================ */
/* تحرير سياق ARP كامل                                              */
/* ================================================================ */
void arp_context_cleanup(void *ctx_ptr)
{
    arp_context_t *ctx = (arp_context_t *)ctx_ptr;
    if (!ctx) return;

    if (ctx->functions)
    {
        for (int i = 0; i < ctx->function_count; i++)
            function_free(&ctx->functions[i]);
        memory_api.free(ctx->functions);
        ctx->functions = NULL;
    }
    ctx->function_count = 0;
    ctx->functions_capacity = 0;

    if (ctx->variables)
    {
        for (int i = 0; i < ctx->variable_count; i++)
        {
            if (ctx->variables[i].name)
                memory_api.free(ctx->variables[i].name);

            arp_value_free(&ctx->variables[i].value);
        }
        memory_api.free(ctx->variables);
        ctx->variables = NULL;
    }
    ctx->variable_count = 0;
    ctx->variables_capacity = 0;

    ctx->active_vm = NULL;

    for (int i = 0; i < ctx->imported_count; i++)
        if (ctx->imported_libs[i])
            memory_api.free(ctx->imported_libs[i]);
    ctx->imported_count = 0;

    ctx->current_window_id = 0;

    memory_api.free(ctx);
}

/* ================================================================ */
/* استخراج اسم الدالة والمعاملات                                     */
/* ================================================================ */
static int parse_function_definition(
    const char *line,
    char **name_out,
    char ***parameters_out,
    int *parameter_count
)
{
    if (!line || !name_out || !parameters_out || !parameter_count)
        return -1;

    const char *p = skip_spaces(line);
    if (!starts_with(p, "دالة(")) return -1;
    p = skip_spaces(skip_prefix(p, "دالة("));

    const char *name_start = p;
    while (*p && *p != ',' && *p != ')') p++;
    const char *name_end = p;
    while (name_end > name_start &&
           (name_end[-1] == ' ' || name_end[-1] == '\t')) name_end--;
    int name_len = name_end - name_start;
    if (name_len <= 0) return -1;

    char *name = arp_strndup(name_start, name_len);
    if (!name) return -1;

    char **params = NULL;
    int count = 0;

    if (*p == ')')
    {
        p = skip_spaces(p + 1);
        if (*p != '\0') { memory_api.free(name); return -1; }
        *name_out = name;
        *parameters_out = NULL;
        *parameter_count = 0;
        return 0;
    }

    if (*p != ',') { memory_api.free(name); return -1; }

    while (*p)
    {
        if (*p == ',') p++;
        p = skip_spaces(p);

        if (*p == ')')
        {
            p = skip_spaces(p + 1);
            if (*p != '\0')
            {
                for (int i = 0; i < count; i++) memory_api.free(params[i]);
                if (params) memory_api.free(params);
                memory_api.free(name);
                return -1;
            }
            break;
        }

        char **np = (char**)memory_api.alloc((count + 1) * sizeof(char*));
        if (!np)
        {
            for (int i = 0; i < count; i++) memory_api.free(params[i]);
            if (params) memory_api.free(params);
            memory_api.free(name);
            return -1;
        }
        for (int i = 0; i < count; i++) np[i] = params[i];
        if (params) memory_api.free(params);
        params = np;

        const char *s = p;
        while (*p && *p != ',' && *p != ')') p++;
        const char *e = p;
        while (e > s && (e[-1] == ' ' || e[-1] == '\t')) e--;
        int len = e - s;
        if (len <= 0)
        {
            for (int i = 0; i < count; i++) memory_api.free(params[i]);
            memory_api.free(params);
            memory_api.free(name);
            return -1;
        }
        params[count] = arp_strndup(s, len);
        if (!params[count])
        {
            for (int i = 0; i < count; i++) memory_api.free(params[i]);
            memory_api.free(params);
            memory_api.free(name);
            return -1;
        }
        count++;

        p = skip_spaces(p);
        if (*p == ',') { continue; }
        if (*p == ')')
        {
            p = skip_spaces(p + 1);
            if (*p != '\0')
            {
                for (int i = 0; i < count; i++) memory_api.free(params[i]);
                memory_api.free(params);
                memory_api.free(name);
                return -1;
            }
            break;
        }
        for (int i = 0; i < count; i++) memory_api.free(params[i]);
        memory_api.free(params);
        memory_api.free(name);
        return -1;
    }

    *name_out = name;
    *parameters_out = params;
    *parameter_count = count;
    return 0;
}

/* ================================================================ */
/* run_line — AOT لكل تعبير مفرد (بديل REPL)                        */
/* ================================================================ */
static int run_line(const char *line)
{
    if (!line) return -1;

    int jerr = 0;
    ast_node_t *jroot = math_parse_ast(line, &jerr);
    if (jerr || !jroot) { ast_free(jroot); return -1; }
    if (!machine_ast_is_jit_supported(jroot)) { ast_free(jroot); return -1; }

    const char *lines[1] = { line };
    void *code = NULL;
    size_t code_size = 0;
    if (machine_jit_compile_function(lines, 1, &code, &code_size) < 0)
    {
        ast_free(jroot);
        return -1;
    }
    ast_free(jroot);

    jit_result_t jr;
    jr.type    = ARP_TYPE_UNKNOWN;
    jr.numeric = 0.0;
    jr.text    = NULL;

    int rc = machine_jit_execute_typed(code, &jr);
    jit_result_free(&jr);
    memory_api.free(code);
    return rc;
}

/* ================================================================ */
/* تحليل "هيكل(الاسم)"                                              */
/* ================================================================ */
static int parse_struct_def_line(const char *line, char **name_out)
{
    if (!line || !name_out) return -1;

    const char *p = skip_spaces(line);
    if (!starts_with(p, "هيكل(")) return -1;
    p = skip_spaces(skip_prefix(p, "هيكل("));

    const char *s = p;
    while (*p && *p != ')') p++;
    const char *e = p;
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) e--;

    int len = e - s;
    if (len <= 0) return -1;

    char *name = arp_strndup(s, len);
    if (!name) return -1;

    p = skip_spaces(p);
    if (*p != ')') { memory_api.free(name); return -1; }
    p = skip_spaces(p + 1);
    if (*p != '\0') { memory_api.free(name); return -1; }

    *name_out = name;
    return 0;
}

/* ================================================================ */
/* تحليل سطر حقل: "اسم = قيمة"                                     */
/* ================================================================ */
static int parse_struct_field_line(
    const char *line,
    char **field_name_out,
    arp_value_t *default_out
)
{
    if (!line || !field_name_out || !default_out) return -1;

    const char *p = skip_spaces(line);
    if (*p == '\0') return -1;

    const char *eq = p;
    while (*eq && *eq != '=') eq++;
    if (*eq != '=') return -1;

    const char *ns = p;
    const char *ne = eq;
    while (ne > ns && (ne[-1] == ' ' || ne[-1] == '\t')) ne--;
    int nlen = ne - ns;
    if (nlen <= 0) return -1;

    char *fname = arp_strndup(ns, nlen);
    if (!fname) return -1;

    const char *vs = skip_spaces(eq + 1);
    if (*vs == '\0') { memory_api.free(fname); return -1; }

    int err = 0;
    ast_node_t *node = math_parse_ast(vs, &err);
    if (err || !node) { ast_free(node); memory_api.free(fname); return -1; }

    arp_value_init(default_out, ARP_TYPE_UNKNOWN);
    if (ast_evaluate_typed(node, default_out, &err) < 0 || err)
    {
        ast_free(node);
        arp_value_free(default_out);
        memory_api.free(fname);
        return -1;
    }
    ast_free(node);

    *field_name_out = fname;
    return 0;
}

static int struct_is_end(const char *line)
{
    if (!line) return 0;
    return strcmp(skip_spaces(line), "نهاية_الهيكل") == 0;
}

/* ================================================================ */
/* execute_script                                                   */
/* ================================================================ */
static int execute_script(const char *script, uint32_t size, int mode)
{
    (void)mode;

    if (!script || size == 0) return -1;

    char *cleaned = NULL;
    uint32_t cleaned_size = 0;

    if (comment_remove(script, size, &cleaned, &cleaned_size) < 0)
        return -1;

    const char *exec_script = cleaned ? cleaned : script;
    uint32_t exec_size = cleaned ? cleaned_size : size;

    char **all_lines = NULL;
    int total = 0, cap = 0;
    uint32_t offset = 0;

    while (offset < exec_size)
    {
        char *line; uint32_t llen;
        if (read_line(exec_script, exec_size, &offset, &line, &llen) < 0)
        {
            free_lines_array_helper(all_lines, total);
            if (cleaned) memory_api.free(cleaned);
            return -1;
        }
        if (!line || llen == 0) { if (line) memory_api.free(line); continue; }

        if (total >= cap)
        {
            int nc = cap ? cap * 2 : 32;
            char **na = (char **)memory_api.alloc(nc * sizeof(char *));
            if (!na)
            {
                memory_api.free(line);
                free_lines_array_helper(all_lines, total);
                if (cleaned) memory_api.free(cleaned);
                return -1;
            }
            for (int i = 0; i < total; i++) na[i] = all_lines[i];
            if (all_lines) memory_api.free(all_lines);
            all_lines = na;
            cap = nc;
        }
        all_lines[total++] = line;
    }

    /* ★★★ معالجة مسبقة: دمج الأسطر الناقصة + تقسيم الأسطر المتعددة ★★★ */
    if (preprocess_script(&all_lines, &total, &cap) < 0)
    {
        free_lines_array_helper(all_lines, total);
        if (cleaned) memory_api.free(cleaned);
        return -1;
    }

    /* ★★★ تضمين المكتبات (مثل #include) ★★★ */
    if (inline_libraries(&all_lines, &total, &cap) < 0)
    {
        free_lines_array_helper(all_lines, total);
        if (cleaned) memory_api.free(cleaned);
        return -1;
    }

    char **main_lines = NULL;
    int main_count = 0, main_cap = 0;
    int had_error = 0;
    int i = 0;

    while (i < total && !had_error)
    {
        const char *p = skip_spaces(all_lines[i]);
        if (*p == '\0') { i++; continue; }

        /* ============ دالة ============ */
        if (starts_with(p, "دالة("))
        {
            char *name = NULL;
            char **params = NULL;
            int param_count = 0;

            if (parse_function_definition(p, &name, &params, &param_count) < 0)
            {
                had_error = 1; break;
            }

            char **body = NULL;
            int body_count = 0, body_cap = 0;
            int found_end = 0;
            int j = i + 1;

            for (; j < total; j++)
            {
                const char *b = skip_spaces(all_lines[j]);
                if (*b == '\0') continue;

                if (starts_with(b, "نهاية("))
                {
                    const char *e = skip_spaces(skip_prefix(b, "نهاية("));
                    const char *es = e;
                    while (*e && *e != ')') e++;
                    int el = e - es;
                    if (el != (int)strlen(name) || strncmp(name, es, el) != 0)
                    { had_error = 1; break; }
                    found_end = 1;
                    break;
                }

                if (body_count >= body_cap)
                {
                    int nc = body_cap ? body_cap * 2 : 16;
                    char **na = (char **)memory_api.alloc(nc * sizeof(char *));
                    if (!na) { had_error = 1; break; }
                    for (int k = 0; k < body_count; k++) na[k] = body[k];
                    if (body) memory_api.free(body);
                    body = na;
                    body_cap = nc;
                }
                body[body_count++] = all_lines[j];
            }

            if (!had_error && !found_end) had_error = 1;

            if (!had_error)
            {
                if (function_register(name,
                                      (const char **)params, param_count,
                                      (const char **)body, body_count) < 0)
                    had_error = 1;
            }

            if (params)
            {
                for (int k = 0; k < param_count; k++) memory_api.free(params[k]);
                memory_api.free(params);
            }
            if (name) memory_api.free(name);
            if (body) memory_api.free(body);

            if (had_error) break;
            i = j + 1;
            continue;
        }

        /* ============ هيكل ============ */
        if (starts_with(p, "هيكل("))
        {
            char *struct_name = NULL;
            if (parse_struct_def_line(p, &struct_name) < 0)
            {
                had_error = 1; break;
            }

            char **fnames = NULL;
            arp_value_t *fdefaults = NULL;
            int fcount = 0, fcap = 0;
            int found_end = 0;
            int j = i + 1;

            for (; j < total; j++)
            {
                const char *b = skip_spaces(all_lines[j]);
                if (*b == '\0') continue;

                if (struct_is_end(b)) { found_end = 1; break; }

                char *fname = NULL;
                arp_value_t fdef;
                arp_value_init(&fdef, ARP_TYPE_UNKNOWN);

                if (parse_struct_field_line(b, &fname, &fdef) < 0)
                {
                    memory_api.free(struct_name);
                    had_error = 1;
                    break;
                }

                if (fcount >= fcap)
                {
                    int nc = fcap ? fcap * 2 : 8;
                    char **nn = (char **)memory_api.alloc(nc * sizeof(char *));
                    arp_value_t *nd = (arp_value_t *)memory_api.alloc(
                                          nc * sizeof(arp_value_t));
                    if (!nn || !nd)
                    {
                        if (nn) memory_api.free(nn);
                        if (nd) memory_api.free(nd);
                        memory_api.free(fname);
                        arp_value_free(&fdef);
                        memory_api.free(struct_name);
                        had_error = 1;
                        break;
                    }
                    /* نقل ملكية العناصر القديمة (بدون نسخ) */
                    for (int k = 0; k < fcount; k++) nn[k] = fnames[k];
                    for (int k = 0; k < fcount; k++) nd[k] = fdefaults[k];
                    if (fnames)    memory_api.free(fnames);
                    if (fdefaults) memory_api.free(fdefaults);
                    fnames    = nn;
                    fdefaults = nd;
                    fcap      = nc;
                }

                fnames[fcount]    = fname;
                fdefaults[fcount] = fdef;
                fcount++;
            }

            if (!had_error && !found_end) had_error = 1;

            if (!had_error)
            {
                if (struct_register(struct_name,
                                    (const char **)fnames,
                                    fdefaults,
                                    fcount) < 0)
                    had_error = 1;
            }

            if (struct_name) memory_api.free(struct_name);
            for (int k = 0; k < fcount; k++)
            {
                memory_api.free(fnames[k]);
                arp_value_free(&fdefaults[k]);
            }
            if (fnames)    memory_api.free(fnames);
            if (fdefaults) memory_api.free(fdefaults);

            if (had_error) break;
            i = j + 1;
            continue;
        }

        /* ============ تصريح بنوع ============ */
        {
            arp_type_t  decl_type;
            char       *decl_name = NULL;
            const char *decl_rhs  = NULL;

            if (declaration_is_start(p, &decl_type, &decl_name, &decl_rhs))
            {
                if (main_count >= main_cap)
                {
                    int nc = main_cap ? main_cap * 2 : 32;
                    char **na = (char **)memory_api.alloc(nc * sizeof(char *));
                    if (!na) { declaration_free(decl_name); had_error = 1; break; }
                    for (int k = 0; k < main_count; k++) na[k] = main_lines[k];
                    if (main_lines) memory_api.free(main_lines);
                    main_lines = na;
                    main_cap = nc;
                }
                main_lines[main_count++] = all_lines[i];
                declaration_free(decl_name);
                i++;
                continue;
            }
        }

        /* ============ سطر عادي → main ============ */
        if (main_count >= main_cap)
        {
            int nc = main_cap ? main_cap * 2 : 32;
            char **na = (char **)memory_api.alloc(nc * sizeof(char *));
            if (!na) { had_error = 1; break; }
            for (int k = 0; k < main_count; k++) na[k] = main_lines[k];
            if (main_lines) memory_api.free(main_lines);
            main_lines = na;
            main_cap = nc;
        }
        main_lines[main_count++] = all_lines[i];
        i++;
    }

    if (!had_error)
    {
        if (main_count > 0)
        {
            if (function_register("__main__", NULL, 0,
                                  (const char **)main_lines,
                                  main_count) < 0)
                had_error = 1;
        }
    }

    free_lines_array_helper(all_lines, total);
    if (main_lines) memory_api.free(main_lines);
    if (cleaned) memory_api.free(cleaned);

    return had_error ? -1 : 0;
}

/* ================================================================ */
/* واجهات التشغيل                                                   */
/* ================================================================ */

static int run_script(const char *script, uint32_t size)
{
    if (!script || size == 0) return -1;

    arp_context_t *ctx = arp_get_current_context();
    if (ctx)
    {
        for (int i = 0; i < ctx->function_count; i++)
            function_free(&ctx->functions[i]);
        if (ctx->functions) memory_api.free(ctx->functions);
        ctx->functions = NULL;
        ctx->function_count = 0;
        ctx->functions_capacity = 0;
    }

    variables_init();
    structs_init();
    libraries_reset();

    void *binary = NULL;
    size_t binary_size = 0;

    if (arp_aot_compile(script, size, &binary, &binary_size) < 0)
        return -1;

    int rc = arp_aot_execute(binary, binary_size);
    memory_api.free(binary);
    return rc;
}

int arp_run_library(const char *script, uint32_t size)
{
    if (!script || size == 0) return -1;

    void *binary = NULL;
    size_t binary_size = 0;
    if (arp_aot_compile(script, size, &binary, &binary_size) < 0)
        return -1;

    int rc = arp_aot_execute_library(binary, binary_size);
    memory_api.free(binary);
    return rc;
}

int arp_compile_only(const char *script, uint32_t size)
{
    if (!script || size == 0) return -1;
    return execute_script(script, size, ARP_MODE_COMPILE);
}

arp_api_t arp_api =
{
    .run_line   = run_line,
    .run_script = run_script
};
