#include <stdint.h>
#include "محرك_المقاطعات/بوابة_المقاطعات.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مكتبة_واجهة_الإدخال_والإخراج.h"
#include "القلب/محركات_الخدمات/محرك_الفيديو/بوابة_الفيديو.h"
#include "القلب/مكتبة_المحركات/مكتبة_معالجة_النصوص.h"
#include "القلب/الجوهرة.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "محرك_العمليات/بوابة_العمليات.h"
#include "محرك_الجدولة/بوابة_الجدولة.h"
#include "القلب/محركات_الخدمات/محرك_التطبيقات/قيم_الملف.h"
#include "القلب/محركات_الخدمات/محرك_التطبيقات/محرك_التطبيقات.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مُتحكم_المقاطعات.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مُتحكم_وموجه_المقاطعات.h"
#include "محرك_الجدولة/مُدير_الأحداث/مُدير_الأحداث.h"
#include "محرك_تواصل_العمليات/محرك_تواصل_العمليات.h"
#include "القلب/محركات_الخدمات/اختبار/مؤشر_الفأرة/مؤشر_الفأرة.h"
#include "القلب/محركات_الخدمات/اختبار/لوحة_المفاتيح/لوحة_المفاتيح.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مؤقت_تنظيم_المقاطعات/المؤقت.h"
#include "القلب/محركات_الخدمات/framebuffer.h"

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289
#define MMIO_BASE 0xFFFF900000000000ULL



extern void isr_timer();
extern void isr_keyboard();
extern void isr_mouse();
extern volatile int need_schedule;


void append_text(const char* str);
void append_hex(uint64_t val);

extern void init_apps();

process_t* idle_p = 0;

/* ================= MULTIBOOT ================= */

struct multiboot2_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot2_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint16_t reserved;
};

struct multiboot2_tag_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
};

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

void append_text(const char* str) {
    int len = 0;
    while (terminal_buffer[len]) len++;
    int i = 0;
    while (str[i] && len < 4094) {
        terminal_buffer[len++] = str[i++];
    }
    terminal_buffer[len] = 0;
}





/* ================= ================= */

void idle() {
    while (1) {
        __asm__("hlt");
    }
}



void append_hex(uint64_t val) {
    char hex[] = "0123456789ABCDEF";
    char buf[17];
    for (int i = 0; i < 16; i++) {
        buf[15 - i] = hex[val & 0xF];
        val >>= 4;
    }
    buf[16] = 0;
    append_text(buf);
}

void page_fault_handler_c(uint64_t* stack) {
    (void)stack;

    uint64_t addr;
    asm("mov %%cr2, %0" : "=r"(addr));

    append_text("PAGE FAULT!\n");
    append_text("Address: 0x");
    append_hex(addr);
    append_text("\n");

    while (1);
}


/* ================= KERNEL MAIN ================= */

void kernel_main(uint64_t magic, uint64_t addr) {

    if ((uint32_t)magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        while (1);
    }
    
    uint8_t* ptr        = (uint8_t*)addr;
    uint32_t total_size = *(uint32_t*)ptr;
    ptr += 8;

    framebuffer_info_t fb_info;
    int fb_found = 0;

    uint8_t* الوسيط_start = 0;
    uint32_t الوسيط_size  = 0;

    while (ptr < (uint8_t*)addr + total_size) {
        struct multiboot2_tag* tag = (struct multiboot2_tag*)ptr;
        if (tag->type == 0) break;

        if (tag->type == 8) {
            struct multiboot2_tag_framebuffer* fb = (void*)tag;
            fb_info.address = fb->framebuffer_addr;
            fb_info.width   = fb->framebuffer_width;
            fb_info.height  = fb->framebuffer_height;
            fb_info.pitch   = fb->framebuffer_pitch;
            fb_info.bpp     = fb->framebuffer_bpp;
            fb_found        = 1;
        }

        if (tag->type == 3) {
            struct multiboot2_tag_module* mod = (void*)tag;
            الوسيط_start = (uint8_t*)(uintptr_t)mod->mod_start;
            الوسيط_size  = mod->mod_end - mod->mod_start;
        }

        ptr += (tag->size + 7) & ~7;
    }

    if (!fb_found) {
        while (1);
    }

    memory_api.init();

    memory_api.map(MMIO_BASE + 0xFEE00000, 0xFEE00000, MEM_WRITE); // LAPIC
    memory_api.map(MMIO_BASE + 0xFEC00000, 0xFEC00000, MEM_WRITE); // IOAPIC

    uint64_t frame = memory_api.alloc_page();
    uint64_t test_addr = 0xFFFF800000400000ULL;
    memory_api.map(test_addr, frame, MEM_WRITE);
    char* test_mem = (char*)test_addr;
    *test_mem = 0x42;

    video_api.init(&fb_info);
    //video_api.clear(0x00224488);
    video_api.swap();

    if (الوسيط_start) {
        parse_الوسيط(
            الوسيط_start,
            الوسيط_size
        );


                /*
 * تشغيل التطبيق تلقائياً
 */

        file_t* auto_app =
        find_file("app.ت");

            if (auto_app)
            {
                app_run(auto_app);

                append_text(
                "[APP] app_m.aros started\n"
            );
        }
        else
        {
            append_text(
                "[APP] app.aros not found\n"
            );
        }
            
    }
/*
    ipc_launch_app(
        "app.aros"
    );
*/
    interrupt_api.init();
    pic_disable();
    interrupt_api.set_gate(
        32,
        (uint64_t)isr_timer
    );
    interrupt_api.set_gate(
        33,
        (uint64_t)isr_keyboard
    );
    interrupt_api.set_gate(
        44,
        (uint64_t)isr_mouse
    );

    extern void isr_page_fault();

    interrupt_api.set_gate(
        14,
        (uint64_t)isr_page_fault
    );


    lapic_init();
    ioapic_init();
    lapic_timer_init();

    interrupt_api.irq_register(0, timer_handler);
    interrupt_api.irq_register(1, keyboard_handler);

    interrupt_api.irq_register(12, mouse_handler);

    mouse_init();
    keyboard_init();

    /* ── idle ── */
    idle_p = process_api.create(idle);

    asm volatile("sti");
    scheduler_api.schedule();

    while (1) {

        asm volatile("hlt");
    }
}
