#include <stdint.h>
#include "القلب/الجوهرة.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "محرك_الجدولة/بوابة_الجدولة.h"
#include "محرك_العمليات/بوابة_العمليات.h"
#include "محرك_المقاطعات/بوابة_المقاطعات.h"
#include "محرك_الجدولة/مُدير_الأحداث/مُدير_الأحداث.h"
#include "القلب/مكتبة_المحركات/مكتبة_معالجة_النصوص.h"
#include "محرك_تواصل_العمليات/محرك_تواصل_العمليات.h"
#include "القلب/الوحدات/وحدات_الخدمات/وحدات_الشاشة/محرك_الفيديو/بوابة_الفيديو.h"
#include "القلب/الوحدات/وحدات_العمليات/محرك_التطبيقات/قيم_الملف.h"
#include "القلب/اختبار/مؤشر_الفأرة/مؤشر_الفأرة.h"
#include "القلب/الوحدات/وحدات_العمليات/محرك_التطبيقات/محرك_التطبيقات.h"
#include "القلب/اختبار/لوحة_المفاتيح/لوحة_المفاتيح.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مُتحكم_المقاطعات.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مُدير_جدول_الواصفات_العام.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مكتبة_واجهة_الإدخال_والإخراج.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مُتحكم_وموجه_المقاطعات.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مؤقت_تنظيم_المقاطعات/المؤقت.h"
#include "القلب/الوحدات/وحدات_الخدمات/وحدات_الشاشة/محرك_العرض/الأدارة/محرك_العرض.h"
#include "القلب/الوحدات/وحدات_الخدمات/وحدات_الشاشة/محرك_العرض/بوابة_العرض.h"

#include "القلب/اختبار/التخزين/sata.h"
#include "القلب/اختبار/طرفية/طرفية.h"
#include "القلب/اختبار/التخزين/نظام_الملفات/نظام_العرب.h"

#define MMIO_BASE 0x3F000000UL



extern void isr_timer();
extern void isr_keyboard();
extern void isr_mouse();
extern volatile int need_schedule;



extern void init_apps();

process_t* idle_p = 0;

/* ================= الوسيط ================= */

file_t files[16];
int file_count = 0;

file_t* init_file = 0;


int hex_to_int(char* str, int len) {
    int val = 0;
    for (int i = 0; i < len; i++) {
        val <<= 4;
        char c = str[i];
        if (c >= '0' && c <= '9') val |= (c - '0');
        else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
    }
    return val;
}

void parse_الوسيط(uint8_t* start, uint32_t size) {
    uint8_t* p   = start;
    uint8_t* end = start + size;

    while (p < end) {
        if (!(p[0]=='0' && p[1]=='7' && p[2]=='0' && p[3]=='7')) break;

        int namesize = hex_to_int((char*)(p + 94), 8);
        int filesize = hex_to_int((char*)(p + 54), 8);

        char*    name = (char*)(p + 110);
        uint8_t* data = (uint8_t*)(p + 110 + namesize);

        if ((uintptr_t)data % 4)
            data += 4 - ((uintptr_t)data % 4);

        if (name[0] && file_count < 16) {
            file_t* f = &files[file_count++];
            int copy_size = namesize - 1;

            if (copy_size > 127)
                copy_size = 127;

            for (int i = 0; i < copy_size; i++){f->name[i] = name[i];}
            f->name[copy_size] = 0;
            f->data = data;
            f->size = filesize;
        }

        if (name[0] == 'T') break;

        p = data + filesize;
        if ((uintptr_t)p % 4)
            p += 4 - ((uintptr_t)p % 4);
    }
}

file_t* find_file(const char* name) {
    for (int i = 0; i < file_count; i++) {
        char* a = files[i].name;
        int j = 0;
        while (name[j] && a[j] && name[j] == a[j]) j++;
        if (name[j] == 0 && a[j] == 0) return &files[i];
    }
    return 0;
}

/* ================= طرفية ================= */

int win;
char command[128];
int cmd_len = 0;
char terminal_buffer[4096];

int starts_with(const char* str, const char* prefix) {
    int i = 0;
    while (prefix[i]) {
        if (str[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}



/* ================= ================= */

void idle() {
    while (1) {
        __asm__("hlt");
    }
}


void page_fault_handler_c(uint64_t* stack) {
    (void)stack;

    uint64_t addr;
    asm("mov %%cr2, %0" : "=r"(addr));

    while (1);
}



void task_entry(void)
{
    while (1)
    {
        display_api.set_background((ticks << 8) & 0x00FFFFFF);

        display_api.render();
    }
}


/* ================= KERNEL MAIN ================= */
 
void KernelMain(boot_info_t *boot){
    asm volatile("cli");
 
    //draw_string(20, 20, "[01] KernelMain", 0x00FFFFFF);
 
    gdt_init();
    //draw_string(20, 40, "[02] GDT OK", 0x00FFFFFF);
 
    uint8_t* الوسيط_start = 0;
    uint32_t الوسيط_size  = 0;
 
    //draw_string(20, 100, "[04] Variables OK", 0x00FFFFFF);
 
    memory_api.init();
    //draw_string(20, 120, "[05] Memory Init OK", 0x00FFFFFF);
 
    تهيئة_الفيديو(boot);
    display_init();

/*


    if (sata_init() == 0)
        {

        terminal_print("yes");

        const uint64_t test_lba = 100;

        uint8_t *write_buffer = (uint8_t *)memory_api.alloc_page();
        uint8_t *read_buffer  = (uint8_t *)memory_api.alloc_page();

        for (int i = 0; i < 512; i++)
            write_buffer[i] = (uint8_t)(i & 0xFF);

        terminal_print("Calling Write");

        int w = sata_write_sector(test_lba, write_buffer);

        if (w == 0)
            terminal_print("Write OK");
        else
            terminal_print("Write Failed");

        memset(read_buffer, 0, 512);

        terminal_print("Calling Read");

        int r = sata_read_sector(test_lba, read_buffer);

        if (r == 0)
            terminal_print("Read OK");
        else
            terminal_print("Read Failed");

        if (w == 0 && r == 0)
        {
            int match = 1;

            for (int i = 0; i < 512; i++)
            {
                if (write_buffer[i] != read_buffer[i])
                {
                    match = 0;
                    break;
                }
            }

            if (match)
                terminal_print("Data Verified: MATCH");
            else
                terminal_print("Data Verified: MISMATCH");
        }

    }
        else
        {

        terminal_print("no");

     }



    terminal_print("SATA Ready");


    if (arabfs_init() == 0)
    {
        terminal_print("ArabFS Init OK");
    }
    else
    {
        terminal_print("ArabFS Init Failed");
    }
*/

    //draw_string(20, 60, "[03] Video OK", 0x00FFFFFF);
 
    //draw_string(20, 80, "Hello Kernel!", 0x00FFFFFF);
 
    memory_api.map(MMIO_BASE + 0xFEE00000, 0xFEE00000, MEM_WRITE);
    //draw_string(20, 140, "[06] LAPIC Map OK", 0x00FFFFFF);
 
    memory_api.map(MMIO_BASE + 0xFEC00000, 0xFEC00000, MEM_WRITE);
    //draw_string(20, 160, "[07] IOAPIC Map OK", 0x00FFFFFF);
 
    uint64_t frame = memory_api.alloc_page();
    //draw_string(20, 180, "[08] alloc_page OK", 0x00FFFFFF);
 
    uint64_t test_addr = 0x3F000000UL;
 
    memory_api.map(test_addr, frame, MEM_WRITE);
    //draw_string(20, 200, "[09] Test Map OK", 0x00FFFFFF);
 
    char* test_mem = (char*)test_addr;
    *test_mem = 0x42;
    //draw_string(20, 220, "[10] Test Write OK", 0x00FFFFFF);
 
    interrupt_api.init();
    //draw_string(20, 240, "[11] IDT Init OK", 0x00FFFFFF);
 
    pic_disable();
    //draw_string(20, 260, "[12] PIC Disabled", 0x00FFFFFF);
 
    interrupt_api.set_gate(32, (uint64_t)isr_timer);
    //draw_string(20, 280, "[13] Timer Gate OK", 0x00FFFFFF);
 
    interrupt_api.set_gate(33, (uint64_t)isr_keyboard);
    //draw_string(20, 300, "[14] Keyboard Gate OK", 0x00FFFFFF);
 
    interrupt_api.set_gate(44, (uint64_t)isr_mouse);
    //draw_string(20, 320, "[15] Mouse Gate OK", 0x00FFFFFF);
 
    extern void isr_page_fault();
 
    interrupt_api.set_gate(14, (uint64_t)isr_page_fault);
    //draw_string(20, 340, "[16] PageFault Gate OK", 0x00FFFFFF);
 
    lapic_init();
    //draw_string(20, 360, "[17] LAPIC Init OK", 0x00FFFFFF);
 
    ioapic_init();
    //draw_string(20, 380, "[18] IOAPIC Init OK", 0x00FFFFFF);
 
    lapic_timer_init();
    //draw_string(20, 400, "[19] LAPIC Timer OK", 0x00FFFFFF);
 
    interrupt_api.irq_register(0, timer_handler);
    //draw_string(20, 420, "[20] Timer IRQ OK", 0x00FFFFFF);
 
    interrupt_api.irq_register(1, keyboard_handler);
    //draw_string(20, 440, "[21] Keyboard IRQ OK", 0x00FFFFFF);
 
    interrupt_api.irq_register(12, mouse_handler);
    //draw_string(20, 460, "[22] Mouse IRQ OK", 0x00FFFFFF);
 
    mouse_init();
    //draw_string(20, 480, "[23] Mouse Init OK", 0x00FFFFFF);
 
    keyboard_init();
    //draw_string(20, 500, "[24] Keyboard Init OK", 0x00FFFFFF);
 
    //video_api.clear(0x00224488);
    //draw_string(20, 20, "[25] Screen Cleared", 0x00FFFFFF);
 
    idle_p = process_api.create(idle);
    //draw_string(20, 40, "[26] Idle Process OK", 0x00FFFFFF);
 
    asm volatile("sti");
    //draw_string(20, 60, "[27] STI OK", 0x00FFFFFF);
    process_t* p = process_api.create(task_entry);
    scheduler_api.add(p);
 
    scheduler_api.schedule();
    //draw_string(20, 80, "[28] Scheduler Returned", 0x00FFFFFF);
 
    while (1)
    {
        asm volatile("hlt");
    }
}
