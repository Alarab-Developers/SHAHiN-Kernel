#include "قيم_الملف.h"
#include "محمل_التطبيقات.h"
#include "القلب/الوحدات/اللغة/لغة.h"
#include "محرك_العمليات/بوابة_العمليات.h"
#include "سياق_اللغة.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "ادوات/شاشة_التطوير/شاشة_التطوير.h"
#include "القلب/الوحدات/اللغة/طبقة_الترجمة/مجمع_الترجمة.h"

extern file_t *find_file(const char *name);

static void diag_print(const char *msg)
{
    (void)msg;
}

static void diag_print_int(const char *label, int value)
{
    (void)label;
    (void)value;
}

static int normalize_path(const char *source, char *destination, int size)
{
    int i = 0;
    int j = 0;

    if (!source || !destination || size <= 1)
        return -1;

    if (source[0] == '.' && (source[1] == '\\' || source[1] == '/'))
        i = 2;

    while (source[i] && j < size - 1)
    {
        destination[j] = (source[i] == '\\') ? '/' : source[i];
        i++;
        j++;
    }

    destination[j] = '\0';
    return 0;
}

static int is_arp_executable(const uint8_t *data, uint32_t size)
{
    if (!data || size < sizeof(arp_exec_header_t)) return 0;
    const arp_exec_header_t *hdr = (const arp_exec_header_t *)data;
    return (hdr->magic == ARP_EXEC_MAGIC && hdr->version == ARP_EXEC_VERSION);
}

static int is_arp_script_file(const char *name)
{
    if (!name) return 0;
    const char *dot = 0;
    for (const char *p = name; *p; p++) if (*p == '.') dot = p;
    if (!dot) return 0;
    return (strcmp(dot, ".تطبيق") == 0 || strcmp(dot, ".لغة") == 0);
;
}

static void arp_process_entry(void *arg)
{
    file_t *f = (file_t *)arg;

    if (f) {
        if (is_arp_executable(f->data, f->size)) {
            if (arp_aot_execute(f->data, f->size) < 0) {
            }
        }
        else if (arp_api.run_script) {
            int r = arp_api.run_script((const char *)f->data, f->size);
            if (r < 0) {
            }
        }
    }

    process_api.exit();
    while (1) asm volatile("hlt");
}

typedef struct {
    void          *binary;
    size_t         size;
    arp_context_t *ctx;
} aot_arg_t;

static void arp_aot_process_entry(void *arg)
{
    aot_arg_t *a = (aot_arg_t *)arg;

    if (!a) {
        process_api.exit();
        while (1) asm volatile("hlt");
    }

    if (a->ctx) {
        arp_set_override_context(a->ctx);
    }

    int rc = arp_aot_execute(a->binary, a->size);

    if (rc < 0) {
    }

    if (a->binary) memory_api.free(a->binary);
    memory_api.free(a);

    arp_set_override_context(NULL);

    process_api.exit();
    while (1) asm volatile("hlt");
}

int loder_run_script(const char *name)
{
    if (!name || !name[0]) return -1;
    if (!is_arp_script_file(name)) return -1;

    char path[256];
    if (normalize_path(name, path, sizeof(path)) < 0)
        return -1;

    file_t *f = find_file(path);
    if (!f) return -1;

    if (is_arp_executable(f->data, f->size))
        return arp_aot_execute(f->data, f->size);

    if (!arp_api.run_script) return -1;
    return arp_api.run_script((const char *)f->data, f->size);
}

int loder_start_script(const char *name)
{
    if (!name || !name[0]) {
        return -1;
    }
    if (!is_arp_script_file(name)) {
        return -1;
    }

    char path[256];
    if (normalize_path(name, path, sizeof(path)) < 0) {
        return -1;
    }

    development_screen_print("[LODER] path: ");
    development_screen_println(path);
    development_screen_update();

    file_t *f = find_file(path);
    if (!f) {
        return -1;
    }

    arp_context_t *cur_ctx = arp_get_current_context();

    if (!cur_ctx) {
        cur_ctx = memory_api.alloc(sizeof(arp_context_t));
        if (!cur_ctx) {
            return -1;
        }
        memset(cur_ctx, 0, sizeof(arp_context_t));

        process_t *cur = process_api.current();
        if (cur) {
            process_api.set_app_context(cur, cur_ctx);
        } else {
            arp_set_current_context(cur_ctx);
        }
    }

    if (is_arp_executable(f->data, f->size)) {
        void *copy = memory_api.alloc(f->size);
        if (!copy) {
            return -1;
        }
        memcpy(copy, f->data, f->size);

        aot_arg_t *arg = memory_api.alloc(sizeof(aot_arg_t));
        if (!arg) {
            memory_api.free(copy);
            return -1;
        }

        arp_context_t *new_ctx = memory_api.alloc(sizeof(arp_context_t));
        if (!new_ctx) {
            memory_api.free(copy);
            memory_api.free(arg);
            return -1;
        }
        memset(new_ctx, 0, sizeof(arp_context_t));

        arg->binary = copy;
        arg->size   = f->size;
        arg->ctx    = new_ctx;

        process_t *p = process_api.create_ex(arp_aot_process_entry, arg);
        if (!p) {
            memory_api.free(arg);
            memory_api.free(copy);
            memory_api.free(new_ctx);
            return -1;
        }

        process_api.set_app_context(p, new_ctx);
        process_api.start(p);

        return 0;
    }

    void *binary = NULL;
    size_t binary_size = 0;

    arp_context_t *ctx = arp_get_current_context();
    if (!ctx) {
        return -1;
    }

    arp_function_t *sv_fns  = ctx->functions;
    int             sv_fc   = ctx->function_count;
    int             sv_fcap = ctx->functions_capacity;
    variable_t     *sv_vars = ctx->variables;
    int             sv_vc   = ctx->variable_count;
    int             sv_vcap = ctx->variables_capacity;

    ctx->functions = NULL; ctx->function_count = 0; ctx->functions_capacity = 0;
    ctx->variables = NULL; ctx->variable_count = 0; ctx->variables_capacity = 0;

    int rc = arp_aot_compile((const char *)f->data, f->size,
                             &binary, &binary_size);

    for (int i = 0; i < ctx->function_count; i++) function_free(&ctx->functions[i]);
    if (ctx->functions) memory_api.free(ctx->functions);
    for (int i = 0; i < ctx->variable_count; i++) {
        if (ctx->variables[i].name) memory_api.free(ctx->variables[i].name);
        /* ★ القيمة النوعية الجديدة: النص مُخزَّن داخل arp_value_t */
        arp_value_free(&ctx->variables[i].value);
    }

    if (ctx->variables) memory_api.free(ctx->variables);

    ctx->functions = sv_fns; ctx->function_count = sv_fc; ctx->functions_capacity = sv_fcap;
    ctx->variables = sv_vars; ctx->variable_count = sv_vc; ctx->variables_capacity = sv_vcap;

    if (rc < 0 || !binary) {
        if (binary) memory_api.free(binary);
        return -1;
    }

    aot_arg_t *arg = memory_api.alloc(sizeof(aot_arg_t));
    if (!arg) {
        memory_api.free(binary);
        return -1;
    }

    arp_context_t *new_ctx = memory_api.alloc(sizeof(arp_context_t));
    if (!new_ctx) {
        memory_api.free(binary);
        memory_api.free(arg);
        return -1;
    }
    memset(new_ctx, 0, sizeof(arp_context_t));

    arg->binary = binary;
    arg->size   = binary_size;
    arg->ctx    = new_ctx;

    process_t *p = process_api.create_ex(arp_aot_process_entry, arg);
    if (!p) {
        memory_api.free(binary);
        memory_api.free(arg);
        memory_api.free(new_ctx);
        return -1;
    }

    process_api.set_app_context(p, new_ctx);
    process_api.start(p);

    return 0;
}

void loder_auto_start(void)
{
    loder_start_script(AUTO_APP_NAME);
}

int loader_app(const char *name)
{
    return loder_start_script(name);
}
