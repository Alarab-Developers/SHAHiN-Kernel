#include <string.h>
#include "مدير_الصفحات.h"
#include "محرك_الذاكرة/محرك_الذاكرة.h"

#define TABLES            4
#define HIGHER_HALF_BASE 0

#define PML4_INDEX(x) (((x) >> 39) & 0x1FF)
#define PDPT_INDEX(x) (((x) >> 30) & 0x1FF)
#define PD_INDEX(x)   (((x) >> 21) & 0x1FF)
#define PT_INDEX(x)   (((x) >> 12) & 0x1FF)

#define ENTRIES 512

static uint64_t pml4_table[ENTRIES] __attribute__((aligned(4096)));
static uint64_t pdpt_table[ENTRIES] __attribute__((aligned(4096)));
static uint64_t pd_tables[TABLES][ENTRIES] __attribute__((aligned(4096)));

/* Early allocator — قبل تهيئة الـ frame allocator */
#define EARLY_POOL_FRAMES 64
static uint64_t early_pool[EARLY_POOL_FRAMES][512] __attribute__((aligned(4096)));
static int early_pool_used = 0;

static uint64_t early_alloc_frame() {
    if (early_pool_used >= EARLY_POOL_FRAMES) while (1);
    uint64_t* frame = early_pool[early_pool_used++];
    memset(frame, 0, 4096);
    return (uint64_t)frame;
}

uint64_t* kernel_pml4 = 0;

extern uint64_t _kernel_end;

uint64_t get_kernel_end_phys() {
    return (uint64_t)&_kernel_end;
}

/* ================================================================
 * paging_init
 * ================================================================ */
void paging_init(void) {

    memset(pml4_table, 0, sizeof(pml4_table));
    memset(pdpt_table, 0, sizeof(pdpt_table));

    /* Identity Map 0 → 4GB (2MB Huge Pages) */
    for (int t = 0; t < TABLES; t++) {

        memset(pd_tables[t], 0, sizeof(pd_tables[t]));

        uint64_t pd_phys = (uint64_t)pd_tables[t];

        pdpt_table[t] =
            pd_phys |
            PAGE_PRESENT |
            PAGE_WRITE;
    }

    uint64_t pdpt_phys = (uint64_t)pdpt_table;

    pml4_table[0] =
        pdpt_phys |
        PAGE_PRESENT |
        PAGE_WRITE;

    for (int t = 0; t < TABLES; t++) {

        for (int i = 0; i < ENTRIES; i++) {

            uint64_t addr =
                ((uint64_t)t * 0x40000000ULL) +
                ((uint64_t)i * 0x200000ULL);

            pd_tables[t][i] =
                addr |
                PAGE_PRESENT |
                PAGE_WRITE |
                PAGE_HUGE;
        }
    }

    kernel_pml4 = pml4_table;
}

/* ================================================================
 * map_page — 4KB mapping
 * pml4: عنوان virtual لجدول PML4
 * ================================================================ */
void map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags) {

    if (!(pml4[PML4_INDEX(virt)] & PAGE_PRESENT)) {
        uint64_t pdpt_phys = alloc_frame();
        uint64_t* pdpt_ptr = (uint64_t*)(HIGHER_HALF_BASE + pdpt_phys);
        memset(pdpt_ptr, 0, 4096);
        pml4[PML4_INDEX(virt)] =
            pdpt_phys | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
    }

    uint64_t* pdpt =
        (uint64_t*)(HIGHER_HALF_BASE +
        (pml4[PML4_INDEX(virt)] & ~0xFFFULL));

    if (!(pdpt[PDPT_INDEX(virt)] & PAGE_PRESENT)) {
        uint64_t pd_phys = alloc_frame();
        uint64_t* pd_ptr = (uint64_t*)(HIGHER_HALF_BASE + pd_phys);
        memset(pd_ptr, 0, 4096);
        pdpt[PDPT_INDEX(virt)] =
            pd_phys | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
    }

    uint64_t* pd =
        (uint64_t*)(HIGHER_HALF_BASE +
        (pdpt[PDPT_INDEX(virt)] & ~0xFFFULL));

    if (!(pd[PD_INDEX(virt)] & PAGE_PRESENT)) {
        uint64_t pt_phys = alloc_frame();
        uint64_t* pt_ptr = (uint64_t*)(HIGHER_HALF_BASE + pt_phys);
        memset(pt_ptr, 0, 4096);
        pd[PD_INDEX(virt)] =
            pt_phys | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
    }

    uint64_t* pt =
        (uint64_t*)(HIGHER_HALF_BASE +
        (pd[PD_INDEX(virt)] & ~0xFFFULL));

    pt[PT_INDEX(virt)] = (phys & ~0xFFFULL) | flags | PAGE_PRESENT;
}

/* ================================================================
 * load_cr3
 *
 * يقبل عنوان VIRTUAL لجدول PML4 (pml4_table في .bss)
 * ويحوّله للفيزيائي بطرح HIGHER_HALF_BASE.
 *
 * استخدم هذه الدالة فقط لجداول الكيرنل الثابتة
 * التي عناوينها في .bss (virtual).
 * ================================================================ */
void load_cr3(uint64_t* pml4) {
    uint64_t phys = (uint64_t)pml4;
    __asm__ __volatile__(
        "mov %0, %%cr3"
        :
        : "r"(phys)
        : "memory"
    );
}

/* ================================================================
 * load_cr3_phys   ← جديد
 *
 * يقبل عنوان فيزيائي مباشرةً ويضعه في CR3 بدون أي تحويل.
 *
 * استخدم هذه الدالة لـ:
 * - p->pml4 (عنوان فيزيائي من alloc_frame)
 * - api_switch_address_space
 * - isr_timer عند تبديل العمليات
 * ================================================================ */
void load_cr3_phys(uint64_t phys) {
    __asm__ __volatile__(
        "mov %0, %%cr3"
        :
        : "r"(phys)
        : "memory"
    );
}

uint64_t* paging_get_pml4() {
    return kernel_pml4;
}

void copy_kernel_mappings(uint64_t* pml4) {
    uint64_t* k = kernel_pml4;
    pml4[0] = k[0];
}
