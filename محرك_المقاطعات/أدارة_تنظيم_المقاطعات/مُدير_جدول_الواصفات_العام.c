#include "مُدير_جدول_الواصفات_العام.h"

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

extern void load_gdt(uint64_t);

static gdt_entry_t gdt[3];
static gdt_ptr_t   gdt_ptr;

static void gdt_set_entry(
    int n,
    uint32_t base,
    uint32_t limit,
    uint8_t access,
    uint8_t gran
) {
    gdt[n].limit_low   = limit        & 0xFFFF;
    gdt[n].base_low    = base         & 0xFFFF;
    gdt[n].base_mid    = (base >> 16) & 0xFF;
    gdt[n].access      = access;
    gdt[n].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[n].base_high   = (base >> 24) & 0xFF;
}

void gdt_init() {

    gdt_set_entry(0, 0, 0, 0,    0);      /* 0x00: null            */
    gdt_set_entry(1, 0, 0, 0x9A, 0xA0);   /* 0x08: كود النواة       */
    gdt_set_entry(2, 0, 0, 0x92, 0xC0);   /* 0x10: بيانات النواة    */

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint64_t)&gdt;

    load_gdt((uint64_t)&gdt_ptr);
}
