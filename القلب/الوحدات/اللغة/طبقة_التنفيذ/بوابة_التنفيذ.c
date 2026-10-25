#include "بوابة_التنفيذ.h"
#include "../لغة.h"
#include "../ادوات/ادوات.h"
#include "القلب/الوحدات/محرك_الفيديو/بوابة_الفيديو.h"
#include "القلب/الوحدات/محرك_الفيديو/محرك_الفيديو.h"
#include "القلب/الوحدات/محمل_التطبيقات/محمل_التطبيقات.h"
#include "../مفاهيم_اللغة/رياضيات.h"
#include "../طبقة_الترجمة/المحلل.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مكتبة_واجهة_الإدخال_والإخراج.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مؤقت_تنظيم_المقاطعات/المؤقت.h"
#include "القلب/اختبار/مؤشر_الفأرة/مؤشر_الفأرة.h"
#include "../مفاهيم_اللغة/المتغيرات.h"
#include "القلب/اختبار/لوحة_المفاتيح/لوحة_المفاتيح.h"
#include "ادوات/شاشة_التطوير/شاشة_التطوير.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "القلب/الوحدات/محمل_التطبيقات/قيم_الملف.h"
#include "القلب/الوحدات/محمل_التطبيقات/سياق_اللغة.h"

/* ============================================================
 * التطبيقات
 * ============================================================ */

int execute_app(const char *name)
{
    if (!name || !name[0]) return -1;
    return loader_app(name);
}

/* ============================================================
 * العتاد
 * ============================================================ */

int execute_hardware(uint16_t port, uint8_t value) { outb(port, value); return 0; }
uint8_t execute_read_hardware(uint16_t port) { return inb(port); }

/* ============================================================
 * الذاكرة
 * ============================================================ */

int execute_memory_write (uint64_t a, uint32_t v) { return memory_raw_write32(a, v); }
int execute_memory_read  (uint64_t a, uint32_t *v){ return memory_raw_read32(a, v); }
int execute_memory_write8(uint64_t a, uint8_t  v) { return memory_raw_write8(a, v); }
int execute_memory_read8 (uint64_t a, uint8_t  *v){ return memory_raw_read8(a, v); }
int execute_memory_write16(uint64_t a, uint16_t v){ return memory_raw_write16(a, v); }
int execute_memory_read16 (uint64_t a, uint16_t *v){ return memory_raw_read16(a, v); }
int execute_memory_write64(uint64_t a, uint64_t v){ return memory_raw_write64(a, v); }
int execute_memory_read64 (uint64_t a, uint64_t *v){ return memory_raw_read64(a, v); }

int execute_memory_copy(uint64_t dest, uint64_t src, uint64_t size)
{ return memory_raw_copy(dest, src, size); }

int execute_memory_set(uint64_t address, uint8_t value, uint64_t size)
{ return memory_raw_set(address, value, size); }

int execute_memory_compare(uint64_t addr1, uint64_t addr2, uint64_t size, int *result)
{ return memory_raw_compare(addr1, addr2, size, result); }

int execute_memory_alloc(uint64_t size, uint64_t *address)
{
    if (!address) return -1;
    uint64_t addr = memory_raw_alloc(size);
    if (addr == 0) return -1;
    *address = addr;
    return 0;
}

int execute_memory_set32(uint64_t address, uint32_t value, uint64_t count)
{
    if (address == 0) return -1;
    uint32_t *p = (uint32_t *)(uintptr_t)address;
    for (uint64_t i = 0; i < count; i++) p[i] = value;
    return 0;
}

int execute_memory_free(uint64_t address) { return memory_raw_free(address); }

int execute_memory_alloc_page(uint64_t *address)
{
    if (!address) return -1;
    *address = memory_raw_alloc_page();
    return (*address != 0) ? 0 : -1;
}

int execute_memory_map(uint64_t virt, uint64_t phys, uint64_t flags)
{ return memory_raw_map(virt, phys, flags); }

/* ============================================================
 * الوقت
 * ============================================================ */

int execute_wait(uint64_t count)
{
    if (count == 0) return 0;
    mouse_dx = 0;
    mouse_dy = 0;
    uint64_t start = timer_get_ticks();
    while ((timer_get_ticks() - start) < count) {
        asm volatile("hlt");
    }
    return 0;
}

/* ============================================================
 * لوحة المفاتيح والإدخال
 * ============================================================ */

int execute_keyboard_value(unsigned char key) { return keyboard_is_pressed(key); }

int execute_input(const char *var_name)
{
    if (!var_name) return -1;

    char buffer[128];
    int  pos = 0;
    char c;
    buffer[0] = '\0';

    while (1)
    {
        do { c = keyboard_read(); } while (c == 0);

        if (c == '\n' || c == '\r')
        {
            development_screen_print("\n");
            break;
        }

        if (c == '\b')
        {
            if (pos > 0)
            {
                pos--;
                /* تراجع عن بايتات UTF-8 continuation */
                while (pos > 0 &&
                       (((unsigned char)buffer[pos]) & 0xC0) == 0x80)
                    pos--;
                buffer[pos] = '\0';
                development_screen_print("\b \b");
            }
            continue;
        }

        unsigned char uc = (unsigned char)c;
        int is_visible = (uc >= 32 && uc <= 126) || (uc >= 0x80);
        if (is_visible && pos < (int)sizeof(buffer) - 4)
        {
            buffer[pos++] = c;
            buffer[pos]   = '\0';

            /* اطبع الحرف المدخل مباشرة */
            char tmp[2] = { c, '\0' };
            development_screen_print(tmp);
        }
    }

    buffer[pos] = '\0';

    /* ===== التخزين النهائي ===== */
    int error = 0;
    ast_node_t *root = math_parse_ast(buffer, &error);

    if (error || !root)
    {
        arp_value_t v;
        arp_value_init(&v, ARP_TYPE_UNKNOWN);
        arp_value_make_text(&v, buffer, -1);
        int rc = variable_set_value(var_name, &v);
        arp_value_free(&v);
        return rc;
    }

    if (root->type == AST_VARIABLE)
    {
        arp_value_t check;
        arp_value_init(&check, ARP_TYPE_UNKNOWN);
        if (variable_get_value(root->name, &check) < 0)
        {
            arp_value_free(&check);
            ast_free(root);
            arp_value_t v;
            arp_value_init(&v, ARP_TYPE_UNKNOWN);
            arp_value_make_text(&v, buffer, -1);
            int rc = variable_set_value(var_name, &v);
            arp_value_free(&v);
            return rc;
        }
        arp_value_free(&check);
    }

    if (root->type == AST_STRING)
    {
        arp_value_t tmp;
        arp_value_init(&tmp, ARP_TYPE_UNKNOWN);
        int e2 = 0;
        if (ast_evaluate_typed(root, &tmp, &e2) == 0 && !e2 &&
            tmp.type == ARP_TYPE_TEXT)
        {
            int rc = variable_set_value(var_name, &tmp);
            arp_value_free(&tmp);
            ast_free(root);
            return rc;
        }
        arp_value_free(&tmp);
        ast_free(root);
        arp_value_t v;
        arp_value_init(&v, ARP_TYPE_UNKNOWN);
        arp_value_make_text(&v, buffer, -1);
        int rc = variable_set_value(var_name, &v);
        arp_value_free(&v);
        return rc;
    }

    arp_value_t result;
    arp_value_init(&result, ARP_TYPE_UNKNOWN);
    if (ast_evaluate_typed(root, &result, &error) < 0)
    {
        ast_free(root);
        arp_value_t v;
        arp_value_init(&v, ARP_TYPE_UNKNOWN);
        arp_value_make_text(&v, buffer, -1);
        int rc = variable_set_value(var_name, &v);
        arp_value_free(&v);
        return rc;
    }
    ast_free(root);

    if (error)
    {
        arp_value_free(&result);
        arp_value_t v;
        arp_value_init(&v, ARP_TYPE_UNKNOWN);
        arp_value_make_text(&v, buffer, -1);
        int rc = variable_set_value(var_name, &v);
        arp_value_free(&v);
        return rc;
    }

    int rc = variable_set_value(var_name, &result);
    arp_value_free(&result);
    return rc;
}

/* ============================================================
 * الوقت الحقيقي (RTC)
 * ============================================================ */

int execute_rtc_get(int field, int *value)
{
    if (!value) return -1;

    /* انتظر حتى يصبح RTC جاهزًا */
    outb(0x70, 0x0A);
    while (inb(0x71) & 0x80) { }

    uint8_t reg;
    switch (field) {
        case 0: reg = 0x00; break;
        case 1: reg = 0x02; break;
        case 2: reg = 0x04; break;
        case 3: reg = 0x07; break;
        case 4: reg = 0x08; break;
        case 5: reg = 0x09; break;
        default: return -1;
    }

    outb(0x70, reg);
    uint8_t v = inb(0x71);

    *value = ((v >> 4) & 0x0F) * 10 + (v & 0x0F);
    return 0;
}

/* ============================================================
 * النصوص
 * ============================================================ */

int execute_text_len(const char *text) { return text ? (int)strlen(text) : 0; }

int execute_text_char(const char *text, int index)
{
    if (!text || index < 0) return -1;
    int len = (int)strlen(text);
    if (index >= len) return -1;
    return (unsigned char)text[index];
}

/* ============================================================
 * الملفات
 * ============================================================ */

uint64_t execute_file_pointer(const char *name)
{
    if (!name || !name[0]) return 0;
    file_t *f = find_file(name);
    if (!f || !f->data) return 0;
    return (uint64_t)(uintptr_t)f->data;
}

/* ============================================================
 * الطباعة
 * ============================================================ */

int execute_print_inline_color(const char *text, uint32_t color)
{
    (void)color;
    if (!text) text = "";
    development_screen_print(text);
    return 0;
}

int execute_print_inline(const char *text)
{
    return execute_print_inline_color(text, 0);
}

int execute_print_line_color(const char *text, uint32_t color)
{
    (void)color;
    if (!text) text = "";
    development_screen_print(text);
    development_screen_print("\n");
    return 0;
}

int execute_print_line(const char *text)
{
    return execute_print_line_color(text, 0);
}

/* ============================================================
 * الفيديو والبافر
 * ============================================================ */

int execute_present_screen(void)
{
    video_api.swap();
    return 0;
}

uint64_t execute_get_back_buffer_ptr(void)
{
    return (uint64_t)(uintptr_t)fb_get_back_buffer();
}

uint64_t execute_get_front_buffer_ptr(void)
{
    return (uint64_t)(uintptr_t)fb_get_front_buffer();
}

uint8_t execute_byte_at(uint64_t address)
{
    if (address == 0) return 0;
    return *((uint8_t *)(uintptr_t)address);
}
