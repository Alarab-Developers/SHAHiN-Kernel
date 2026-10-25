#include "سياق_اللغة.h"
#include "محرك_العمليات/بوابة_العمليات.h"
#include "ادوات/شاشة_التطوير/شاشة_التطوير.h"


static arp_context_t *g_arp_override_ctx = NULL;
static arp_context_t *g_local_ctx = NULL;


arp_context_t* arp_get_current_context(void)
{
    if (g_arp_override_ctx) {
        return g_arp_override_ctx;
    }

    if (g_local_ctx) {
        return g_local_ctx;
    }

    process_t* p = process_api.current();
    if (!p)
        return 0;

    return (arp_context_t*)
        process_api.get_app_context(p);
}

void arp_set_current_context(arp_context_t *ctx)
{
    g_local_ctx = ctx;
}

/* ★ جديد: تثبيت سياق مؤقت بأولوية عالية (للـ AOT execution) */
void arp_set_override_context(arp_context_t *ctx)
{
    g_arp_override_ctx = ctx;
}
