#include "مُدير_جدول_المقاطعات.h"

idt_entry_t idt[IDT_ENTRIES];
idt_ptr_t   idt_ptr;

extern void load_idt(uint64_t);

/*
 * idt_set_gate — ضبط إدخال في IDT (Kernel gate، DPL=0)
 * n       : رقم المقاطعة (0-255)
 * handler : عنوان دالة المعالج (64-bit)
 */
void idt_set_gate(int n, uint64_t handler) {
    idt[n].offset_low  =  handler        & 0xFFFF;
    idt[n].selector    = 0x08;           /* kernel code segment */
    idt[n].ist         = 0;
    idt[n].type_attr   = 0x8E;           /* Present, DPL=0, Interrupt Gate */
    idt[n].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].zero        = 0;
}

void idt_set_user_gate(int n, uint64_t handler) {
    idt[n].offset_low  =  handler        & 0xFFFF;
    idt[n].selector    = 0x08;
    idt[n].ist         = 0;
    idt[n].type_attr   = 0xEE;           /* Present, DPL=3, Interrupt Gate */
    idt[n].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].zero        = 0;
}

void idt_init() {
    /* صفّر الجدول كله أولاً، مع وضع كل الإدخالات Not Present */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt[i].offset_low  = 0;
        idt[i].selector    = 0x08;
        idt[i].ist         = 0;
        idt[i].type_attr   = 0x0E;   /* Not Present بدل 0x8E */
        idt[i].offset_mid  = 0;
        idt[i].offset_high = 0;
        idt[i].zero        = 0;
    }

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint64_t)&idt;

    load_idt((uint64_t)&idt_ptr);
}
