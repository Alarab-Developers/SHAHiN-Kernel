#include "مدير_الكومة.h"
#include "محرك_الذاكرة/الأدارة/مدير_الصفحات.h"
#include "محرك_الذاكرة/الأدارة/مدير_الاطار.h"
#include "محرك_الذاكرة/محرك_الذاكرة.h"




/* =========================================================
 * Heap State
 * ========================================================= */

static uint64_t heap_current = HEAP_START;
static uint64_t heap_end     = HEAP_START;





/* =========================================================
 * heap_expand
 * ========================================================= */

static int heap_expand() {

    uint64_t frame = alloc_frame();

    if (!frame)
        return 0;

    uint64_t* k_pml4 = paging_get_pml4();

    map_page(
        k_pml4,
        heap_end,
        frame,
        PAGE_PRESENT | PAGE_WRITE
    );

    heap_end += PAGE_SIZE;

    return 1;
}

/* =========================================================
 * heap_init
 * ========================================================= */

void heap_init() {



    uint64_t* k_pml4 = paging_get_pml4();

    /*
     * أول صفحة للهيب
     */
    uint64_t frame = alloc_frame();

    if (!frame)
        return;

    map_page(
        k_pml4,
        HEAP_START,
        frame,
        PAGE_PRESENT | PAGE_WRITE
    );

    heap_current = HEAP_START;
    heap_end     = HEAP_START + PAGE_SIZE;
}

/* =========================================================
 * kmalloc
 * ========================================================= */

void* kmalloc(size_t size) {

    if (size == 0)
        return 0;

    /*
     * إصلاح alignment:
     *
     * المشكلة القديمة:
     *   الـ header = sizeof(size_t) = 8 bytes
     *   → ptr = heap_current + 8
     *   → ptr % 16 = 8  (غير محاذٍ!)
     *   → p->rsp يُكتب في عنوان غير محاذٍ
     *   → iretq يقرأ RSP خاطئاً → crash
     *
     * الإصلاح:
     *   الـ header = 16 bytes ثابتة
     *   → ptr = heap_current + 16
     *   → ptr محاذٍ لـ 16 دائماً 
     */
    #define KMALLOC_HEADER_SIZE 16

    /* 16-byte alignment للحجم */
    if (size % 16 != 0)
        size += 16 - (size % 16);

    /* Header (16) + payload */
    size_t total = size + KMALLOC_HEADER_SIZE;

    /* Align heap_current لـ 16 */
    if (heap_current % 16 != 0)
        heap_current += 16 - (heap_current % 16);

    /* وسّع الهيب عند الحاجة */
    while (heap_current + total > heap_end) {

        if (!heap_expand())
            return 0;
    }

    /* Header: نكتب الحجم في أول 8 bytes */
    size_t* header = (size_t*)heap_current;
    *header = size;

    /*
     * User pointer: يبدأ بعد الـ 16 bytes
     * heap_current محاذٍ لـ 16 → ptr محاذٍ لـ 16 
     */
    void* ptr = (void*)(heap_current + KMALLOC_HEADER_SIZE);

    /* صفّر الذاكرة */
    memset(ptr, 0, size);

    /* حرّك المؤشر */
    heap_current += total;

    return ptr;
}

/* =========================================================
 * kfree
 * ========================================================= */

void kfree(void* ptr) {

    /*
     * allocator بسيط (bump allocator)
     * لا يدعم free حالياً
     */

    (void)ptr;
}
