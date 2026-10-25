#include "بوابة_الذاكرة.h"
#include "محرك_الذاكرة.h"
#include "الأدارة/مدير_الكومة.h"
#include "الأدارة/مدير_الصفحات.h"
#include "الأدارة/مدير_الاطار.h"
#include "الأدارة/مدير_فضاء_العناوين.h"

/*
بوابة الذاكرة هي الواجهة الرئيسية التي تربط جميع مكونات النواة بشكل غير مباشر بأدارة الذاكرة
وتكون عادة تصريحات مع استدعاءات لدوال المحرك لخدمة طلبات المكونات التي بحاجة لها لكي تعمل وتكون عادة اجزاء من المقاطعات والجدولة والعمليات 
والمحرك يقدم تحكم مباشر في الذاكر عبر (ادارة وأنشاء وتخصيص  الصفحات والعناوين الفيزيائية وخرائط الذاكرة)

*/



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


/* ================================================================ */
/* كتابة مباشرة إلى عنوان ذاكرة                                    */
/* ================================================================ */

int memory_raw_write32(
    uint64_t address,
    uint32_t value
)
{
    if (address == 0)
        return -1;

    volatile uint32_t *ptr =
        (volatile uint32_t *)(uintptr_t)address;

    *ptr = value;

    return 0;
}


/* ================================================================ */
/* قراءة مباشرة من عنوان ذاكرة                                     */
/* ================================================================ */

int memory_raw_read32(
    uint64_t address,
    uint32_t *value
)
{
    if (address == 0 || !value)
        return -1;

    volatile uint32_t *ptr =
        (volatile uint32_t *)(uintptr_t)address;

    *value = *ptr;

    return 0;
}

/* ================================================================ */
/* قراءة/كتابة 8 بت                                                 */
/* ================================================================ */
int memory_raw_write8(uint64_t address, uint8_t value)
{
    if (address == 0)
        return -1;

    volatile uint8_t *ptr = (volatile uint8_t *)(uintptr_t)address;
    *ptr = value;
    return 0;
}

int memory_raw_read8(uint64_t address, uint8_t *value)
{
    if (address == 0 || !value)
        return -1;

    volatile uint8_t *ptr = (volatile uint8_t *)(uintptr_t)address;
    *value = *ptr;
    return 0;
}

/* ================================================================ */
/* قراءة/كتابة 16 بت                                                */
/* ================================================================ */
int memory_raw_write16(uint64_t address, uint16_t value)
{
    if (address == 0)
        return -1;

    volatile uint16_t *ptr = (volatile uint16_t *)(uintptr_t)address;
    *ptr = value;
    return 0;
}

int memory_raw_read16(uint64_t address, uint16_t *value)
{
    if (address == 0 || !value)
        return -1;

    volatile uint16_t *ptr = (volatile uint16_t *)(uintptr_t)address;
    *value = *ptr;
    return 0;
}

/* ================================================================ */
/* قراءة/كتابة 64 بت                                                */
/* ================================================================ */
int memory_raw_write64(uint64_t address, uint64_t value)
{
    if (address == 0)
        return -1;

    volatile uint64_t *ptr = (volatile uint64_t *)(uintptr_t)address;
    *ptr = value;
    return 0;
}

int memory_raw_read64(uint64_t address, uint64_t *value)
{
    if (address == 0 || !value)
        return -1;

    volatile uint64_t *ptr = (volatile uint64_t *)(uintptr_t)address;
    *value = *ptr;
    return 0;
}

/* ================================================================ */
/* نسخ / تعبئة / مقارنة كتلة ذاكرة (بايت بايت، آمنة على MMIO)       */
/* ================================================================ */
int memory_raw_copy(uint64_t dest, uint64_t src, uint64_t size)
{
    if (dest == 0 || src == 0) return -1;

    /* نسخ 32-bit — آمن على MMIO */
    uint32_t *d32 = (uint32_t *)(uintptr_t)dest;
    uint32_t *s32 = (uint32_t *)(uintptr_t)src;
    uint64_t count = size / 4;
    
    for (uint64_t i = 0; i < count; i++)
        d32[i] = s32[i];
    
    /* الباقي بايت بايت */
    uint8_t *d8 = (uint8_t *)(uintptr_t)dest;
    uint8_t *s8 = (uint8_t *)(uintptr_t)src;
    for (uint64_t i = count * 4; i < size; i++)
        d8[i] = s8[i];
    
    return 0;
}

int memory_raw_set(uint64_t address, uint8_t value, uint64_t size)
{
    if (address == 0)
        return -1;

    volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)address;

    for (uint64_t i = 0; i < size; i++)
        p[i] = value;

    return 0;
}

int memory_raw_compare(uint64_t addr1, uint64_t addr2, uint64_t size, int *result)
{
    if (addr1 == 0 || addr2 == 0 || !result)
        return -1;

    volatile uint8_t *a = (volatile uint8_t *)(uintptr_t)addr1;
    volatile uint8_t *b = (volatile uint8_t *)(uintptr_t)addr2;

    for (uint64_t i = 0; i < size; i++) {
        if (a[i] != b[i]) {
            *result = (a[i] < b[i]) ? -1 : 1;
            return 0;
        }
    }

    *result = 0;
    return 0;
}

/* ================================================================ */
/* تخصيص/تحرير من الكومة — أغلفة رقيقة فوق memory_api                */
/* ================================================================ */
uint64_t memory_raw_alloc(uint64_t size)
{
    if (size == 0)
        return 0;

    void *ptr = memory_api.alloc((size_t)size);
    return (uint64_t)(uintptr_t)ptr;
}

int memory_raw_free(uint64_t address)
{
    if (address == 0)
        return -1;

    memory_api.free((void *)(uintptr_t)address);
    return 0;
}

/* ================================================================ */
/* تخصيص صفحة فيزيائية، وربط عنوان افتراضي بعنوان فيزيائي            */
/* ================================================================ */
uint64_t memory_raw_alloc_page(void)
{
    return memory_api.alloc_page();
}

int memory_raw_map(uint64_t virt, uint64_t phys, uint64_t flags)
{
    memory_api.map(virt, phys, flags);
    return 0;
}

static void api_destroy_address_space(address_space_t *space) {
    address_space_destroy(space);
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
    .destroy_address_space     = api_destroy_address_space,
    .switch_address_space      = api_switch_address_space,
    .get_current_address_space = api_get_current_address_space,
};
