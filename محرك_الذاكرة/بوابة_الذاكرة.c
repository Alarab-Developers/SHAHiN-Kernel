#include "بوابة_الذاكرة.h"
#include "محرك_الذاكرة.h"
#include "الأدارة/مدير_الكومة.h"
#include "الأدارة/مدير_الصفحات.h"
#include "الأدارة/مدير_الاطار.h"
#include "الأدارة/مدير_فضاء_العناوين.h"

/* ================================================================
 * pml4 الكيرنل — عنوان فيزيائي
 * يُحفظ هنا بعد paging_init لاستخدامه في api_map
 * ================================================================ */
static uint64_t kernel_pml4_phys = 0;

/* ================================================================
 * api_alloc_page
 * ================================================================ */
static uint64_t api_alloc_page() {
    return alloc_frame();
}

/* ================================================================
 * api_map — map في address space الكيرنل الحالي
 * pml4 الكيرنل هو virtual pointer (pml4_table في .bss)
 * ================================================================ */
static void api_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    map_page(paging_get_pml4(), virt, phys, flags);
}

/* ================================================================
 * api_map_to — map في address space مُحدد
 * target: عنوان virtual لجدول PML4
 * ================================================================ */
static void api_map_to(uint64_t* target, uint64_t virt,
                        uint64_t phys, uint64_t flags) {
    map_page(target, virt, phys, flags);
}

static void api_init(void) {
    paging_init();

    uint64_t* virt_pml4 = paging_get_pml4();


    kernel_pml4_phys = (uint64_t)virt_pml4;


    load_cr3_phys(kernel_pml4_phys);

    memory_init();
    address_space_init();
}

static uint64_t* api_get_pml4() {
    return paging_get_pml4();
}



static address_space_t *api_create_address_space(void)
{
    return address_space_create();
}

static void api_switch_address_space(address_space_t *space)
{
    address_space_switch(space);
}

static address_space_t *api_get_current_address_space(void)
{
    return address_space_current();
}


/* ================================================================
 * memory_api
 * ================================================================ */
memory_api_t memory_api = {
    .init                      = api_init,
    .alloc                     = kmalloc,
    .free                      = kfree,
    .alloc_page                = api_alloc_page,
    .map                       = api_map,
    .map_to                    = api_map_to,
    .get_pml4                  = api_get_pml4,
    .create_address_space      = api_create_address_space,
    .switch_address_space      = api_switch_address_space,
    .get_current_address_space = api_get_current_address_space,
};
