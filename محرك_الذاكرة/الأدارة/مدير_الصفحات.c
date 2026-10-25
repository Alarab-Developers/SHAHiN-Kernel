#include "مدير_الصفحات.h"
#include "محرك_الذاكرة/محرك_الذاكرة.h"
#include "محرك_الذاكرة/الأدارة/مدير_الاطار.h"   


#define TABLES            64    /* كفّ 64 GB من الـ identity map (كان 4 = 4GB) */
#define HIGHER_HALF_BASE  0

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
 *
 * يبني identity map من 0 إلى حجم الرام الفعلي (مقرَّبًا لأعلى لأقرب
 * 1 GB)، بسقف TABLES × 1GB = 64 GB.
 * ================================================================ */
void paging_init(void)
{
    memset(pml4_table, 0, sizeof(pml4_table));
    memset(pdpt_table, 0, sizeof(pdpt_table));

    /* كم GB نحتاج لتغطية الرام الفعلية؟ */
    uint64_t ram = g_total_ram_bytes;

    /* إن لم يُحدَّد حجم الرام (احتياطيًا) نستخدم 4 GB كسلوك قديم */
    if (ram < 0x100000000ULL)
        ram = 0x100000000ULL;

    uint64_t gb_count = (ram + 0x3FFFFFFFULL) / 0x40000000ULL;  /* تقريب لأعلى */
    if (gb_count > TABLES) gb_count = TABLES;
    if (gb_count < 1)      gb_count = 1;

    /* ربط PDPT → PD tables */
    for (uint64_t t = 0; t < gb_count; t++) {
        memset(pd_tables[t], 0, sizeof(pd_tables[t]));

        uint64_t pd_phys = (uint64_t)pd_tables[t];
        pdpt_table[t] = pd_phys | PAGE_PRESENT | PAGE_WRITE;
    }

    uint64_t pdpt_phys = (uint64_t)pdpt_table;
    pml4_table[0] = pdpt_phys | PAGE_PRESENT | PAGE_WRITE;

    /* تعبئة PD tables بـ huge pages (2 MB) */
    for (uint64_t t = 0; t < gb_count; t++) {
        for (int i = 0; i < ENTRIES; i++) {
            uint64_t addr =
                (t * 0x40000000ULL) + ((uint64_t)i * 0x200000ULL);

            pd_tables[t][i] =
                addr | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE;
        }
    }

    kernel_pml4 = pml4_table;
}

/* ================================================================
 * map_page — 4 KB mapping
 * ================================================================ */
void map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags)
{
    /* ── PML4 ── */
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

    /* ── PDPT ── */
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

    /* ── PD — فصل الصفحة الضخمة إن لزم ── */
    uint64_t pd_entry = pd[PD_INDEX(virt)];

    if (!(pd_entry & PAGE_PRESENT)) {
        uint64_t pt_phys = alloc_frame();
        uint64_t* pt_ptr = (uint64_t*)(HIGHER_HALF_BASE + pt_phys);
        memset(pt_ptr, 0, 4096);
        pd[PD_INDEX(virt)] =
            pt_phys | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
    }
    else if (pd_entry & PAGE_HUGE) {
        uint64_t huge_phys  = pd_entry & ~0x1FFFFFULL;
        uint64_t huge_flags = (pd_entry & 0xFFFULL) & ~PAGE_HUGE;

        uint64_t pt_phys = alloc_frame();
        uint64_t* pt_ptr = (uint64_t*)(HIGHER_HALF_BASE + pt_phys);
        memset(pt_ptr, 0, 4096);

        for (uint64_t i = 0; i < 512; i++) {
            pt_ptr[i] = (huge_phys + i * 0x1000ULL)
                        | huge_flags
                        | PAGE_PRESENT;
        }

        pd[PD_INDEX(virt)] =
            pt_phys | huge_flags | PAGE_PRESENT;
    }

    uint64_t* pt =
        (uint64_t*)(HIGHER_HALF_BASE +
        (pd[PD_INDEX(virt)] & ~0xFFFULL));

    pt[PT_INDEX(virt)] = (phys & ~0xFFFULL) | flags | PAGE_PRESENT;
}

/* ================================================================
 * load_cr3 / load_cr3_phys / paging_get_pml4 / copy_kernel_mappings
 * ================================================================ */
void load_cr3(uint64_t* pml4) {
    uint64_t phys = (uint64_t)pml4;
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(phys) : "memory");
}

void load_cr3_phys(uint64_t phys) {
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(phys) : "memory");
}

uint64_t* paging_get_pml4() { return kernel_pml4; }

void copy_kernel_mappings(uint64_t* pml4) {
    uint64_t* k = kernel_pml4;
    pml4[0] = k[0];
}
