#include "المنفذ.h"
#include "../لغة.h"
#include "../ادوات/ادوات.h"
#include "القلب/الوحدات/محرك_الفيديو/بوابة_الفيديو.h"
#include "القلب/الوحدات/محرك_الفيديو/محرك_الفيديو.h"
#include "القلب/الوحدات/محمل_التطبيقات/محمل_التطبيقات.h"
#include "../المفكر/مفاهيم_اللغة/رياضيات.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مكتبة_واجهة_الإدخال_والإخراج.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مؤقت_تنظيم_المقاطعات/المؤقت.h"
#include "القلب/اختبار/مؤشر_الفأرة/مؤشر_الفأرة.h"
#include "../المفكر/مفاهيم_اللغة/المتغيرات.h"
#include "القلب/اختبار/لوحة_المفاتيح/لوحة_المفاتيح.h"
#include "القلب/الوحدات/شاشة_التطوير/شاشة_التطوير.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "القلب/الوحدات/محمل_التطبيقات/قيم_الملف.h"

int execute_background(uint32_t color) {
    video_api.clear(color);
    return 0;
}

int execute_calculate(double result) {
    char buffer[64];
    double_to_string(result, buffer, 6);
    return 0;
}

int execute_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {
    video_api.fill_triangle(x1, y1, x2, y2, x3, y3, color);
    return 0;
}

int execute_present(void) {
    video_api.swap();
    return 0;
}

int execute_app(const char *name) {
    if (!name || !name[0]) return -1;
    return loader_app(name);
}

int execute_rect(int x, int y, int width, int height, uint32_t color) {
    if (width <= 0 || height <= 0) return -1;
    fill_rect(x, y, width, height, color);
    return 0;
}

int execute_hardware(uint16_t port, uint8_t value) {
    outb(port, value);
    return 0;
}

uint8_t execute_read_hardware(uint16_t port) {
    return inb(port);
}

int execute_memory_write(uint64_t address, uint32_t value) {
    return memory_raw_write32(address, value);
}

int execute_memory_read(uint64_t address, uint32_t *value) {
    return memory_raw_read32(address, value);
}

int execute_memory_write8(uint64_t address, uint8_t value) {
    return memory_raw_write8(address, value);
}

int execute_memory_read8(uint64_t address, uint8_t *value) {
    return memory_raw_read8(address, value);
}

int execute_memory_write16(uint64_t address, uint16_t value) {
    return memory_raw_write16(address, value);
}

int execute_memory_read16(uint64_t address, uint16_t *value) {
    return memory_raw_read16(address, value);
}

int execute_memory_write64(uint64_t address, uint64_t value) {
    return memory_raw_write64(address, value);
}

int execute_memory_read64(uint64_t address, uint64_t *value) {
    return memory_raw_read64(address, value);
}

int execute_memory_copy(uint64_t dest, uint64_t src, uint64_t size) {
    return memory_raw_copy(dest, src, size);
}

int execute_memory_set(uint64_t address, uint8_t value, uint64_t size) {
    return memory_raw_set(address, value, size);
}

int execute_memory_compare(uint64_t addr1, uint64_t addr2, uint64_t size, int *result) {
    return memory_raw_compare(addr1, addr2, size, result);
}

int execute_memory_alloc(uint64_t size, uint64_t *address) {
    if (!address) return -1;
    uint64_t addr = memory_raw_alloc(size);
    if (addr == 0) return -1;
    *address = addr;
    return 0;
}

int execute_memory_free(uint64_t address) {
    return memory_raw_free(address);
}

int execute_memory_alloc_page(uint64_t *address) {
    if (!address) return -1;
    *address = memory_raw_alloc_page();
    return (*address != 0) ? 0 : -1;
}

int execute_memory_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    return memory_raw_map(virt, phys, flags);
}

int execute_wait(uint64_t count) {
    uint64_t start;
    if (count == 0) return 0;
    mouse_dx = 0;
    mouse_dy = 0;
    start = timer_get_ticks();
    while ((timer_get_ticks() - start) < count) {
        asm volatile("hlt");
    }
    return 0;
}

int execute_keyboard_value(unsigned char key) {
    return keyboard_is_pressed(key);
}

int execute_print(const char *text) {
    if (!text) return -1;
    development_screen_print(text);
    return 0;
}

int execute_input(const char *var_name) {
    if (!var_name) return -1;
    char buffer[64];
    int pos = 0;
    char c;
    while (1) {
        do { c = keyboard_read(); } while (c == 0);
        if (c == '\n' || c == '\r') {
            development_screen_print("\n");
            break;
        }
        if (c == '\b') {
            if (pos > 0) { pos--; development_screen_print("\b \b"); }
            continue;
        }
        if (c >= 32 && c <= 126 && pos < 63) {
            buffer[pos++] = c;
            char str[2] = {c, '\0'};
            development_screen_print(str);
        }
    }
    buffer[pos] = '\0';
    int error = 0;
    double value = string_to_double(buffer, NULL);
    if (error) value = 0;
    return variable_set(var_name, value);
}
