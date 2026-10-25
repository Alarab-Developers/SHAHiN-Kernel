#include "مكتبات_اللغة.h"

#include "../لغة.h"

#include "القلب/الوحدات/محمل_التطبيقات/قيم_الملف.h"
#include "القلب/الوحدات/محمل_التطبيقات/سياق_اللغة.h"

#include "../ادوات/ادوات.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"

extern file_t* find_file(const char *name);

/* ================================================================ */
/* سجل المكتبات المستوردة — لكل عملية على حدة                       */
/* ================================================================ */

static arp_context_t *lib_ctx(void)
{
    return arp_get_current_context();
}

void libraries_reset(void)
{
    arp_context_t *ctx = lib_ctx();
    if (!ctx) return;

    for (int i = 0; i < ctx->imported_count; i++)
    {
        if (ctx->imported_libs[i])
        {
            memory_api.free(ctx->imported_libs[i]);
            ctx->imported_libs[i] = NULL;
        }
    }
    ctx->imported_count = 0;
}

static int is_imported(const char *path)
{
    if (!path) return 0;
    arp_context_t *ctx = lib_ctx();
    if (!ctx) return 0;

    for (int i = 0; i < ctx->imported_count; i++)
        if (ctx->imported_libs[i] && strcmp(ctx->imported_libs[i], path) == 0)
            return 1;
    return 0;
}

static int mark_imported(const char *path)
{
    if (!path) return -1;
    arp_context_t *ctx = lib_ctx();
    if (!ctx) return -1;
    if (ctx->imported_count >= ARP_MAX_IMPORTED_LIBS) return -1;

    ctx->imported_libs[ctx->imported_count] = arp_strdup(path);
    if (!ctx->imported_libs[ctx->imported_count]) return -1;

    ctx->imported_count++;
    return 0;
}

static int is_arp_library(const char *name)
{
    if (!name)
        return 0;

    const char *dot = 0;
    for (const char *p = name; *p; p++)
    {
        if (*p == '.')
            dot = p;
    }

    if (!dot)
        return 0;

    return strcmp(dot, ".لغة") == 0;
}

int library_import(const char *name)
{
    if (!name || !name[0])
        return -1;

    if (!is_arp_library(name))
        return -1;

    /* ★ تطبيع المسار إلى جذر الأرشيف (يحذف "./" و ".\") */
    char path[256];
    arp_normalize_archive_path(name, path, sizeof(path));

    if (path[0] == '\0')
        return -1;

    if (is_imported(path))
        return 0;

    file_t *f = find_file(path);
    if (!f)
        return -1;

    if (!f->data || f->size == 0)
        return -1;

    if (mark_imported(path) < 0)
        return -1;

    return arp_run_library(
        (const char *)f->data,
        f->size
    );
}
