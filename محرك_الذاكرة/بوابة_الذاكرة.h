#ifndef MEMORY_API_H
#define MEMORY_API_H


#include "القلب/المكتبات/المكتبات.h"

#define MEM_PRESENT  1
#define MEM_WRITE    2
#define MEM_USER     4

#include "الأدارة/مدير_فضاء_العناوين.h"



typedef struct {
    // Heap
    void*   (*alloc)(size_t size);
    void    (*free)(void* ptr);

    // Paging
    uint64_t (*alloc_page)();
    void     (*free_page)(uint64_t);

    void (*map)(uint64_t virt, uint64_t phys, uint64_t flags);
    void (*map_to)(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags);


    void (*init)(void);

    uint64_t* (*get_pml4)();

    address_space_t *(*create_address_space)(void);
    
    void (*destroy_address_space)(address_space_t *space);

    void (*switch_address_space)(address_space_t *);

    address_space_t *(*get_current_address_space)(void);


} memory_api_t;

extern memory_api_t memory_api;
int memory_raw_write32(uint64_t address, uint32_t value);
int memory_raw_read32(uint64_t address, uint32_t *value);

/* ================================================================
 * قراءة/كتابة بأحجام مختلفة (8/16/64 بت) — 32 بت أعلاه
 * ================================================================ */
int memory_raw_write8(uint64_t address, uint8_t value);
int memory_raw_read8(uint64_t address, uint8_t *value);
int memory_raw_write16(uint64_t address, uint16_t value);
int memory_raw_read16(uint64_t address, uint16_t *value);
int memory_raw_write64(uint64_t address, uint64_t value);
int memory_raw_read64(uint64_t address, uint64_t *value);

/* ================================================================
 * عمليات على كتل الذاكرة (نسخ / تعبئة / مقارنة)
 * ================================================================ */
int memory_raw_copy(uint64_t dest, uint64_t src, uint64_t size);
int memory_raw_set(uint64_t address, uint8_t value, uint64_t size);
int memory_raw_compare(uint64_t addr1, uint64_t addr2, uint64_t size, int *result);

/* ================================================================
 * تخصيص/تحرير من الكومة، وتخصيص صفحة فيزيائية، وربط virt<->phys
 * ================================================================ */
uint64_t memory_raw_alloc(uint64_t size);
int memory_raw_free(uint64_t address);
uint64_t memory_raw_alloc_page(void);
int memory_raw_map(uint64_t virt, uint64_t phys, uint64_t flags);

#endif
