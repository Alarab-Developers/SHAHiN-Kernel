#include "مدير_فضاء_العناوين.h"
#include "مدير_الصفحات.h"
#include "مدير_الاطار.h"
#include "مدير_الكومة.h"

static address_space_t kernel_space;
static address_space_t *current_space = 0;

void address_space_init(void)
{
    kernel_space.pml4 = paging_get_pml4();
    current_space = &kernel_space;
}

address_space_t *address_space_create(void)
{
    address_space_t *space = kmalloc(sizeof(address_space_t));
    if (!space) return 0;

    uint64_t phys = alloc_frame();
    if (!phys) return 0;

    uint64_t *pml4 = (uint64_t *)phys;
    memset(pml4, 0, 4096);
    copy_kernel_mappings(pml4);  // ينسخ المدخل 0 فقط

    space->pml4 = pml4;
    return space;
}

/* =========================================================
 * address_space_destroy  ←  تم التنفيذ الكامل
 * ========================================================= */
void address_space_destroy(address_space_t *space)
{
    if (!space || !space->pml4) return;

    uint64_t *pml4 = space->pml4;

    // نبدأ من المدخل 1 (نتجاهل المدخل 0 الخاص بالنواة)
    for (int i = 1; i < 512; i++) {
        if (!(pml4[i] & PAGE_PRESENT)) continue;

        uint64_t pdpt_phys = pml4[i] & ~0xFFFULL;
        uint64_t *pdpt = (uint64_t *)(uintptr_t)pdpt_phys;  // HIGHER_HALF_BASE = 0

        for (int j = 0; j < 512; j++) {
            if (!(pdpt[j] & PAGE_PRESENT)) continue;

            uint64_t pd_phys = pdpt[j] & ~0xFFFULL;
            uint64_t *pd = (uint64_t *)(uintptr_t)pd_phys;

            for (int k = 0; k < 512; k++) {
                if (!(pd[k] & PAGE_PRESENT)) continue;

                if (pd[k] & PAGE_HUGE) {
                    // صفحة ضخمة 2MB
                    uint64_t phys_addr = pd[k] & ~0x1FFFFFULL;
                    free_frame(phys_addr);
                } else {
                    // صفحة عادية 4KB
                    uint64_t pt_phys = pd[k] & ~0xFFFULL;
                    uint64_t *pt = (uint64_t *)(uintptr_t)pt_phys;

                    for (int l = 0; l < 512; l++) {
                        if (!(pt[l] & PAGE_PRESENT)) continue;
                        uint64_t phys_addr = pt[l] & ~0xFFFULL;
                        free_frame(phys_addr);
                    }
                    free_frame(pt_phys);  // تحرير جدول الصفحات نفسه
                }
            }
            free_frame(pd_phys);  // تحرير جدول الدليل
        }
        free_frame(pdpt_phys);  // تحرير جدول PDPT
    }

    free_frame((uint64_t)pml4);  // تحرير جدول PML4 نفسه

    // تحرير كتلة address_space من الكومة
    kfree(space);
}

void address_space_switch(address_space_t *space)
{
    if (!space) return;
    current_space = space;
    load_cr3_phys((uint64_t)space->pml4);
}

address_space_t *address_space_current(void)
{
    return current_space;
}
