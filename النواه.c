#include "القلب/الجوهرة.h"
#include "القلب/المكتبات/انواع.h"
#include "القلب/معلومات_المقلع.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "محرك_الذاكرة/الأدارة/مدير_الاطار.h"
#include "محرك_الجدولة/بوابة_الجدولة.h"
#include "محرك_العمليات/بوابة_العمليات.h"
#include "محرك_المقاطعات/بوابة_المقاطعات.h"
#include "محرك_الجدولة/الأدارة/مُدير_الأحداث.h"
#include "القلب/المكتبات/المكتبات.h"
#include "القلب/الوحدات/محرك_الفيديو/بوابة_الفيديو.h"
#include "القلب/الوحدات/محرك_الفيديو/محرك_الفيديو.h"
#include "القلب/الوحدات/محمل_التطبيقات/قيم_الملف.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مُتحكم_المقاطعات.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مُدير_جدول_الواصفات_العام.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مكتبة_واجهة_الإدخال_والإخراج.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مُتحكم_وموجه_المقاطعات.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مؤقت_تنظيم_المقاطعات/المؤقت.h"

#include "القلب/الوحدات/التخزين/ساتا.h"
#include "القلب/اختبار/شاشة_الخطأ/اختبار_اول.h"
#include "ادوات/شاشة_التطوير/شاشة_التطوير.h"
#include "القلب/الوحدات/التخزين/نظام_الملفات/نظام_العرب.h"

#include "القلب/الوحدات/محمل_التطبيقات/محمل_التطبيقات.h"
#include "القلب/الوحدات/الشاشة_الحرجة/الشاشة_الحرجة.h"
#include "القلب/الوحدات/اللغة/لغة.h"
#include "القلب/الوحدات/اللغة/طبقة_الترجمة/مجمع_الترجمة.h"

process_t* idle_p = 0;

extern uint8_t _bss_start[];
extern uint8_t _bss_end[];

static void clear_bss(void)
{
    uint8_t *p = _bss_start;
    uint8_t *e = _bss_end;
    while (p < e) *p++ = 0;
}

/* ================= Archive ================= */

#define MAX_ARCHIVE_FILES 30000

file_t files[MAX_ARCHIVE_FILES];
int file_count = 0;

file_t* init_file = 0;


/* tar (ustar) numeric fields are stored as ASCII octal text */
uint32_t oct_to_uint(const char* str, int len) {
    uint32_t val = 0;
    for (int i = 0; i < len; i++) {
        char c = str[i];
        if (c < '0' || c > '7') break;
        val = (val << 3) + (uint32_t)(c - '0');
    }
    return val;
}

#define TAR_BLOCK_SIZE 512

#define TAR_OFF_NAME     0
#define TAR_LEN_NAME     100
#define TAR_OFF_SIZE     124
#define TAR_LEN_SIZE     12
#define TAR_OFF_TYPEFLAG 156

void parse_archive(uint8_t* start, uint32_t size)
{
    if (!start || size < TAR_BLOCK_SIZE)
        return;

    uint8_t* p   = start;
    uint8_t* end = start + size;

    while (p + TAR_BLOCK_SIZE <= end)
    {
        int all_zero = 1;

        for (int i = 0; i < TAR_BLOCK_SIZE; i++)
        {
            if (p[i] != 0)
            {
                all_zero = 0;
                break;
            }
        }

        if (all_zero)
            break;

        char* name      = (char*)(p + TAR_OFF_NAME);
        char typeflag   = (char)p[TAR_OFF_TYPEFLAG];

        uint32_t filesize = oct_to_uint(
            (char*)(p + TAR_OFF_SIZE),
            TAR_LEN_SIZE
        );

        uint32_t data_blocks =
            (filesize + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE;
        uint32_t data_bytes = data_blocks * TAR_BLOCK_SIZE;

        uint8_t* data = p + TAR_BLOCK_SIZE;

        if (data > end)
            break;
        if (data_bytes > (uint32_t)(end - data))
            break;

        if ((typeflag == '0' || typeflag == 0) &&
            name[0] != 0 &&
            file_count < MAX_ARCHIVE_FILES)
        {
            file_t* f = &files[file_count];

            int i = 0;
            while (i < TAR_LEN_NAME &&
                   i < (int)sizeof(f->name) - 1 &&
                   name[i] != '\0')
            {
                f->name[i] = name[i];
                i++;
            }
            f->name[i] = '\0';

            if (f->name[0] != '\0')
            {
                f->data = data;
                f->size = filesize;
                file_count++;
            }
        }

        p = data + data_bytes;
    }
}

void list_archive_contents(void) {
    for (int i = 0; i < file_count; i++) {
        char line[160];
        char size_str[16];

        uint32_t v = files[i].size;
        int pos = 0;

        if (v == 0) {
            size_str[pos++] = '0';
        } else {
            char tmp[16];
            int tpos = 0;
            while (v > 0) {
                tmp[tpos++] = (char)('0' + (v % 10));
                v /= 10;
            }
            while (tpos > 0) {
                size_str[pos++] = tmp[--tpos];
            }
        }
        size_str[pos] = '\0';

        strcpy(line, "  ");
        strcat(line, files[i].name);
        strcat(line, " (");
        strcat(line, size_str);
        strcat(line, " bytes)");
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

/* ================= ================= */

void idle() {
    while (1) {
        __asm__("hlt");
    }
}

void page_fault_handler_c(uint64_t* stack)
{
    uint64_t addr;

    asm volatile(
        "mov %%cr2, %0"
        : "=r"(addr)
    );

    panic_page_fault(addr, stack);
}

/* ============================================================
 * Print a 64-bit integer to the development screen
 * ============================================================ */
static void print_u64(uint64_t value)
{
    char buf[24];
    int i = 0;

    if (value == 0)
    {
        development_screen_print("0");
        return;
    }

    while (value > 0 && i < 23)
    {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (i > 0)
    {
        i--;
        char ch[2] = { buf[i], '\0' };
        development_screen_print(ch);
    }
}

static void print_ram_size(uint64_t bytes)
{
    development_screen_print("RAM: ");
    print_u64(bytes);
    development_screen_print(" bytes = ");

    uint64_t mib = bytes / (1024ULL * 1024ULL);
    print_u64(mib);

    development_screen_print(" MiB  (");
    print_u64(mib / 1024);
    development_screen_print(" GiB)");
    development_screen_println("");
}

/* ================= KERNEL MAIN ================= */

void KernelMain(boot_info_t *boot){

    frame_allocator_set_ram_size(boot->total_ram_bytes);

    core_init();
    video_init(boot);
    //development_screen_print("RAM: ");
    //print_u64(boot->total_ram_bytes);
    //development_screen_println("");

    //development_screen_print("FB: ");
    //print_u64((uint64_t)fb_get_back_buffer());
    ///development_screen_println("");
    //development_screen_init();   /* ← لطباعة رسائل التحميل */
    core_memory();               /* ← قبل أي شغل ARP */
    core_interrupt();

    /* =========================================================
     * 1) Unpack the archive
     * ========================================================= */
    if (boot->system_archive && boot->system_archive_size)
    {
        parse_archive((uint8_t *)boot->system_archive,
                      boot->system_archive_size);
    }

    test_erorr_main();

    idle_p = process_api.create(idle);

    loader_app("الاعدادات/ابدء.تطبيق");
    core_run();

    while (1) asm volatile("hlt");
}
