#include "مدير_فضاء_العناوين.h"
#include "مدير_الصفحات.h"
#include "مدير_الاطار.h"
#include "مدير_الكومة.h"

static address_space_t kernel_space;
static address_space_t *current_space = 0;

/* =========================================================
 * address_space_init
 * ========================================================= */

void address_space_init(void)
{
    kernel_space.pml4 = paging_get_pml4();

    current_space = &kernel_space;
}

/* =========================================================
 * address_space_create
 * ========================================================= */

address_space_t *address_space_create(void)
{
    address_space_t *space =
        kmalloc(sizeof(address_space_t));

    if (!space)
        return 0;

    uint64_t phys = alloc_frame();

    if (!phys)
        return 0;

    uint64_t *pml4 = (uint64_t *)phys;

    memset(pml4, 0, 4096);

    copy_kernel_mappings(pml4);

    space->pml4 = pml4;

    return space;
}

/* =========================================================
 * address_space_destroy
 * ========================================================= */

void address_space_destroy(address_space_t *space)
{
    (void)space;

    /* لاحقاً:
     * free page tables
     * free frames
     * free(space)
     */
}

/* =========================================================
 * address_space_switch
 * ========================================================= */

void address_space_switch(address_space_t *space)
{
    if (!space)
        return;

    current_space = space;

    load_cr3_phys((uint64_t)space->pml4);
}

/* =========================================================
 * address_space_current
 * ========================================================= */

address_space_t *address_space_current(void)
{
    return current_space;
}
