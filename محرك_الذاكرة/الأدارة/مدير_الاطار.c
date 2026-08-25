#include "مدير_الاطار.h"
#include "محرك_الذاكرة/محرك_الذاكرة.h"
#include "مدير_الصفحات.h"

static uint64_t first_free_frame = 0;
static uint64_t alloc_base_frame = 0;

static uint64_t last_frame = 0x4000000ULL;

static uint8_t* frame_bitmap = 0;
static uint64_t bitmap_size = 0;

extern uint64_t get_kernel_end_phys();



/* =========================================================
 * frame_allocator_init
 * ========================================================= */

void frame_allocator_init() {

    uint64_t kernel_end = get_kernel_end_phys();

    /*
     * أول frame متاح بعد الكيرنل
     */
    first_free_frame =
        (kernel_end + PAGE_SIZE - 1) &
        ~((uint64_t)PAGE_SIZE - 1);

    /*
     * عدد الـ frames الكلي
     */
    uint64_t total_frames =
        (last_frame - first_free_frame) / PAGE_SIZE;

    /*
     * حجم الـ bitmap بالبايت
     */
    bitmap_size = (total_frames + 7) / 8;

    uint64_t* k_pml4 = paging_get_pml4();

    /*
     * Map bitmap memory
     */
    for (uint64_t offset = 0;
         offset < bitmap_size;
         offset += PAGE_SIZE) {

        uint64_t phys = first_free_frame + offset;

        map_page(
            k_pml4,
            phys,
            phys,
            PAGE_PRESENT | PAGE_WRITE
        );
    }

    /*
     * Virtual address للـ bitmap
     */
    frame_bitmap =
        (uint8_t*)first_free_frame;

    /*
     * تصفير الـ bitmap
     */
    memset(frame_bitmap, 0, bitmap_size);

    /*
     * عدد الـ frames المستخدمة للـ bitmap نفسه
     */
    uint64_t bitmap_frames =
        (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;

    /*
     * احجز frames الخاصة بالـ bitmap
     */
    for (uint64_t i = 0; i < bitmap_frames; i++) {

        uint64_t byte_idx = i / 8;
        uint64_t bit_idx  = i % 8;

        frame_bitmap[byte_idx] |= (1 << bit_idx);
    }

    /*
     * أول frame حقيقي متاح للتخصيص
     */
    alloc_base_frame =
        first_free_frame +
        (bitmap_frames * PAGE_SIZE);
}

/* =========================================================
 * alloc_frame
 * ========================================================= */

uint64_t alloc_frame() {

    if (!frame_bitmap)
        return 0;

    uint64_t total_bits = bitmap_size * 8;

    for (uint64_t i = 0; i < total_bits; i++) {

        uint64_t byte_idx = i / 8;
        uint64_t bit_idx  = i % 8;

        /*
         * frame غير مستخدم
         */
        if (!(frame_bitmap[byte_idx] & (1 << bit_idx))) {

            /*
             * احجزه
             */
            frame_bitmap[byte_idx] |= (1 << bit_idx);

            /*
             * أرجع العنوان الفيزيائي
             */
            return alloc_base_frame + (i * PAGE_SIZE);
        }
    }

    return 0;
}

/* =========================================================
 * reserve_frame_range  ← جديد
 *
 * تحجز نطاق [phys_start, phys_end) في الـ bitmap مباشرة، بحيث
 * لا يعطيه alloc_frame() لأي جهة أخرى لاحقاً. تُستخدم لحماية
 * مناطق ذات عنوان ثابت (مثل منطقة تحميل تطبيقات .bin).
 * ========================================================= */

void reserve_frame_range(uint64_t phys_start, uint64_t phys_end) {

    if (!frame_bitmap)
        return;

    if (phys_start < alloc_base_frame)
        phys_start = alloc_base_frame;

    uint64_t total_bits = bitmap_size * 8;

    for (uint64_t addr = phys_start; addr < phys_end; addr += PAGE_SIZE) {

        uint64_t index = (addr - alloc_base_frame) / PAGE_SIZE;

        if (index >= total_bits)
            break;

        uint64_t byte_idx = index / 8;
        uint64_t bit_idx  = index % 8;

        frame_bitmap[byte_idx] |= (1 << bit_idx);
    }
}
