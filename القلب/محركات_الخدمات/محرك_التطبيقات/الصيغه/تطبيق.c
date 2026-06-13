#include "تطبيق.h"

#include <string.h>
#include <stdint.h>

#include "محرك_العمليات/بوابة_العمليات.h"
#include "محرك_الجدولة/بوابة_الجدولة.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "محرك_تواصل_العمليات/بوابة_النواه.h"

#include "هيكل_التطبيق/رأس_التطبيق.h"
#include "هيكل_التطبيق/بيانات_التطبيق.h"
#include "هيكل_التطبيق/أمتداد_التطبيق.h"

#define APP_BASE      0xFFFFA00000000000ULL
#define PAGE_SIZE     4096ULL

/* ========================================================= */
/* AROS RUN */
/* ========================================================= */

int aros_run(file_t* f)
{
    if (!f)
        return 0;

    if (!f->data)
        return 0;

    if (f->size == 0)
        return 0;

    /*
     * حجز slot
     */

    int slot = app_allocate_slot();

    if (slot < 0)
        return 0;

    /*
     * قراءة Header
     */

    aros_header_t* hdr =
        (aros_header_t*)f->data;

    /*
     * تحقق من الصيغة
     */

    if (!app_format_valid(f))
        return 0;

    /*
     * عدد الصفحات المطلوبة
     */

    uint64_t pages =
        (hdr->data_size + PAGE_SIZE - 1)
        / PAGE_SIZE;

    if (pages == 0)
        return 0;

    /*
     * عنوان افتراضي خاص بالتطبيق
     */

    uint64_t app_base =
        APP_BASE +
        ((uint64_t)slot * 0x1000000ULL);

    /*
     * ربط الصفحات
     */

    for (uint64_t i = 0; i < pages; i++)
    {
        uint64_t phys =
            memory_api.alloc_page();

        if (!phys)
            return 0;

        memory_api.map(
            app_base + (i * PAGE_SIZE),
            phys,
            MEM_WRITE
        );
    }

    /*
     * بداية ذاكرة التطبيق
     */

    void* app_mem =
        (void*)app_base;

    /*
     * نسخ البيانات
     */

    app_data_copy(
        f,
        app_mem
    );

    /*
     * حفظ نقطة التشغيل
     */

    app_set_entry(
        slot,
        (app_entry_t)(
            (uint8_t*)app_mem +
            hdr->entry_point
        )
    );

    /*
     * إنشاء العملية
     */

    process_t* p =
        process_api.create(
            app_get_launcher(slot)
        );

    if (!p)
        return 0;

    /*
     * إضافتها للجدولة
     */

    scheduler_api.add(p);

    return 1;
}
