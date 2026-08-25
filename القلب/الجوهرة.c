#include "الجوهرة.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "محرك_الجدولة/بوابة_الجدولة.h"
#include "محرك_العمليات/بوابة_العمليات.h"
#include "محرك_المقاطعات/بوابة_المقاطعات.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مُدير_جدول_الواصفات_العام.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مُتحكم_وموجه_المقاطعات.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مُتحكم_المقاطعات.h"
#include "القلب/اختبار/مؤشر_الفأرة/مؤشر_الفأرة.h"
#include "القلب/اختبار/لوحة_المفاتيح/لوحة_المفاتيح.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مؤقت_تنظيم_المقاطعات/المؤقت.h"

#define MMIO_BASE 0x3F000000UL

extern void isr_timer();
extern void isr_mouse();
extern void isr_keyboard();

extern void isr_exc_0();   /* #DE  Divide Error */
extern void isr_exc_6();   /* #UD  Invalid Opcode */
extern void isr_exc_8();   /* #DF  Double Fault */
extern void isr_exc_13();  /* #GP  General Protection Fault */


void core_init() {

    gdt_init();

    memory_api.init();

    interrupt_api.init();

    process_api.init();   

    scheduler_api.init();
}

void core_memory() {

    memory_api.map(MMIO_BASE + 0xFEE00000, 0xFEE00000, MEM_WRITE);
 
    memory_api.map(MMIO_BASE + 0xFEC00000, 0xFEC00000, MEM_WRITE);
 
    uint64_t frame = memory_api.alloc_page();

    uint64_t test_addr = 0x3F000000UL;
 
    memory_api.map(test_addr, frame, MEM_WRITE);

    char* test_mem = (char*)test_addr;
    *test_mem = 0x42; 

}


void core_interrupt() {
 
    pic_disable();
 
    interrupt_api.set_gate(32, (uint64_t)isr_timer);
 
    interrupt_api.set_gate(33, (uint64_t)isr_keyboard);
 
    interrupt_api.set_gate(44, (uint64_t)isr_mouse);
    extern void isr_page_fault();
 
    interrupt_api.set_gate(14, (uint64_t)isr_page_fault);


    interrupt_api.set_gate(0,  (uint64_t)isr_exc_0);
    interrupt_api.set_gate(6,  (uint64_t)isr_exc_6);
    interrupt_api.set_gate(8,  (uint64_t)isr_exc_8);
    interrupt_api.set_gate(13, (uint64_t)isr_exc_13);


    lapic_init();

 
    ioapic_init();

 
    lapic_timer_init();

 
    interrupt_api.irq_register(0, timer_handler);
 
    interrupt_api.irq_register(1, keyboard_handler);

    interrupt_api.irq_register(12, mouse_handler);

    mouse_init();
 
    keyboard_init();

}

void core_run() {

    asm volatile("sti");
    scheduler_api.schedule();

}
