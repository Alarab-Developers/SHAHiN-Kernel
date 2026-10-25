#include "مدير_الكومة.h"
#include "محرك_الذاكرة/الأدارة/مدير_الصفحات.h"
#include "محرك_الذاكرة/الأدارة/مدير_الاطار.h"
#include "محرك_الذاكرة/محرك_الذاكرة.h"


typedef struct block block_t;

struct block {
    uint64_t  magic;
    uint64_t  size;
    block_t  *next;
    block_t  *prev;
};

#define BLOCK_USED   0xBEEFCAFE12345678ULL
#define BLOCK_FREE   0xDEADBEEF87654321ULL

#define HEADER_SIZE  sizeof(block_t)    /* 32 bytes */
#define MIN_SPLIT    32                 /* لا تقسّم إن كان الباقي < هذا */

#define ALIGN16(x)   (((x) + 15ULL) & ~15ULL)


/* =========================================================
 * حالة الكومة
 * ========================================================= */

static uint64_t heap_start   = HEAP_START;
static uint64_t heap_current = HEAP_START;
static uint64_t heap_end     = HEAP_START;

static block_t *first_block  = NULL;
static block_t *last_block   = NULL;


/* =========================================================
 * حماية المقاطعة
 * ========================================================= */

static inline uint64_t irq_save(void)
{
    uint64_t flags;
    __asm__ __volatile__("pushfq; pop %0" : "=r"(flags) :: "memory");
    __asm__ __volatile__("cli" ::: "memory");
    return flags;
}

static inline void irq_restore(uint64_t flags)
{
    __asm__ __volatile__("push %0; popfq" :: "r"(flags) : "memory");
}


/* =========================================================
 * تمديد الكومة بصفحة
 * ========================================================= */

static int heap_map_one_page(void)
{
    uint64_t frame = alloc_frame();
    if (!frame) return 0;

    map_page(paging_get_pml4(), heap_end,
             frame, PAGE_PRESENT | PAGE_WRITE);

    heap_end += PAGE_SIZE;
    return 1;
}


/* =========================================================
 * مساعدات الكتل
 * ========================================================= */

static block_t *block_init(uint64_t addr, uint64_t bytes, uint64_t magic)
{
    block_t *b = (block_t *)addr;
    b->magic = magic;
    b->size  = bytes;
    b->next  = NULL;
    b->prev  = NULL;
    return b;
}

/* أضف كتلة حرة في نهاية الكومة (bump) */
static block_t *heap_append(uint64_t bytes)
{
    bytes = ALIGN16(bytes);

    heap_current = ALIGN16(heap_current);

    while (heap_current + bytes > heap_end) {
        if (!heap_map_one_page()) return NULL;
    }

    block_t *b = block_init(heap_current, bytes, BLOCK_FREE);

    b->prev = last_block;
    if (last_block) last_block->next = b;
    else            first_block      = b;
    last_block = b;

    heap_current += bytes;
    return b;
}

/* قسّم كتلة حرة كبيرة إلى مستخدمة + حرة متبقية */
static void block_split(block_t *b, uint64_t payload_needed)
{
    uint64_t total_needed = HEADER_SIZE + payload_needed;

    if (b->size <= total_needed + MIN_SPLIT)
        return;                             /* لا فائدة من التقسيم */

    uint64_t rem_addr = (uint64_t)b + total_needed;
    uint64_t rem_size = b->size - total_needed;

    block_t *rem = block_init(rem_addr, rem_size, BLOCK_FREE);
    rem->prev = b;
    rem->next = b->next;
    if (b->next) b->next->prev = rem;
    else         last_block    = rem;

    b->size = total_needed;
    b->next = rem;
}

/* ادمج مع الكتلة التالية إن كانت حرة */
static void block_coalesce_next(block_t *b)
{
    if (!b->next || b->next->magic != BLOCK_FREE) return;

    block_t *n = b->next;
    b->size += n->size;
    b->next  = n->next;
    if (n->next) n->next->prev = b;
    else         last_block    = b;
}

/* ادمج مع الكتلة السابقة إن كانت حرة */
static void block_coalesce_prev(block_t *b)
{
    if (!b->prev || b->prev->magic != BLOCK_FREE) return;
    block_coalesce_next(b->prev);
}


/* =========================================================
 * heap_init — صفحة واحدة، كتلة حرة واحدة تغطيها
 * ========================================================= */

void heap_init(void)
{
    uint64_t frame = alloc_frame();
    if (!frame) return;

    map_page(paging_get_pml4(), HEAP_START,
             frame, PAGE_PRESENT | PAGE_WRITE);

    heap_start   = HEAP_START;
    heap_end     = HEAP_START + PAGE_SIZE;
    heap_current = HEAP_START;

    block_t *b = block_init(heap_start, PAGE_SIZE, BLOCK_FREE);
    first_block = b;
    last_block  = b;

    heap_current = heap_start + PAGE_SIZE;
}


/* =========================================================
 * kmalloc — first-fit
 * ========================================================= */

void *kmalloc(size_t size)
{
    if (size == 0) return NULL;

    uint64_t payload = ALIGN16((uint64_t)size);

    uint64_t flags = irq_save();

    /* 1) ابحث في الكتل الموجودة */
    for (block_t *b = first_block; b; b = b->next) {
        if (b->magic != BLOCK_FREE) continue;
        if (b->size < HEADER_SIZE + payload) continue;

        block_split(b, payload);
        b->magic = BLOCK_USED;

        void *p = (void *)((uint64_t)b + HEADER_SIZE);

        irq_restore(flags);
        memset(p, 0, size);
        return p;
    }

    /* 2) لم نجد → bump */
    block_t *b = heap_append(HEADER_SIZE + payload);
    if (!b) {
        irq_restore(flags);
        return NULL;
    }

    b->magic = BLOCK_USED;
    void *p = (void *)((uint64_t)b + HEADER_SIZE);

    irq_restore(flags);
    memset(p, 0, size);
    return p;
}


/* =========================================================
 * kfree — علّم كحرة + دمج
 * ========================================================= */

void kfree(void *ptr)
{
    if (!ptr) return;

    uint64_t flags = irq_save();

    block_t *b = (block_t *)((uint64_t)ptr - HEADER_SIZE);

    /* تحقق من النطاق */
    if ((uint64_t)b < heap_start || (uint64_t)b >= heap_end) {
        irq_restore(flags);
        return;
    }

    /* تحقق من أنه كتلة مستخدمة (يمنع double-free) */
    if (b->magic != BLOCK_USED) {
        irq_restore(flags);
        return;
    }

    b->magic = BLOCK_FREE;

    /* ادمج مع الجيران */
    block_coalesce_next(b);
    block_coalesce_prev(b);

    irq_restore(flags);
}
