#include "القلب/الجوهرة.h"
#include "القلب/المكتبات/انواع.h"
#include "القلب/معلومات_المقلع.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "محرك_الجدولة/بوابة_الجدولة.h"
#include "محرك_العمليات/بوابة_العمليات.h"
#include "محرك_المقاطعات/بوابة_المقاطعات.h"
#include "محرك_الجدولة/الأدارة/مُدير_الأحداث.h"
#include "القلب/المكتبات/المكتبات.h"
#include "محرك_تواصل_العمليات/محرك_تواصل_العمليات.h"
#include "القلب/الوحدات/محرك_الفيديو/بوابة_الفيديو.h"
#include "القلب/الوحدات/محمل_التطبيقات/قيم_الملف.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مُتحكم_المقاطعات.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مُدير_جدول_الواصفات_العام.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مكتبة_واجهة_الإدخال_والإخراج.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مُتحكم_وموجه_المقاطعات.h"
#include "محرك_المقاطعات/الأداره_المتقدمة_للمقاطعات/مؤقت_تنظيم_المقاطعات/المؤقت.h"

#include "القلب/الوحدات/التخزين/ساتا.h"
#include "القلب/اختبار/شاشة_الخطأ/اختبار_اول.h"
#include "القلب/الوحدات/شاشة_التطوير/شاشة_التطوير.h"
#include "القلب/الوحدات/التخزين/نظام_الملفات/نظام_العرب.h"


#include "القلب/الوحدات/محمل_التطبيقات/محمل_التطبيقات.h"

#include "القلب/الوحدات/الشاشة_الحرجة/الشاشة_الحرجة.h"


#include "القلب/الوحدات/اللغة/لغة.h"








process_t* idle_p = 0;

/* ================= الوسيط ================= */

#define MAX_ARCHIVE_FILES 128

file_t files[MAX_ARCHIVE_FILES];
int file_count = 0;

file_t* init_file = 0;


/* الحقول الرقمية في رأس tar (ustar) مخزّنة كنص ASCII بصيغة ثمانية (octal) */
uint32_t oct_to_uint(const char* str, int len) {
    uint32_t val = 0;
    for (int i = 0; i < len; i++) {
        char c = str[i];
        if (c < '0' || c > '7') break; /* توقف عند مسافة/NUL في نهاية الحقل */
        val = (val << 3) + (uint32_t)(c - '0');
    }
    return val;
}

#define TAR_BLOCK_SIZE 512

/* أوفستات رأس ustar (512 بايت لكل رأس) */
#define TAR_OFF_NAME     0
#define TAR_LEN_NAME     100
#define TAR_OFF_SIZE     124
#define TAR_LEN_SIZE     12
#define TAR_OFF_TYPEFLAG 156

void parse_الوسيط(uint8_t* start, uint32_t size)
{
    if (!start || size < TAR_BLOCK_SIZE)
        return;

    uint8_t* p   = start;
    uint8_t* end = start + size;

    while (p + TAR_BLOCK_SIZE <= end)
    {
        /* ========================================================= */
        /* التحقق من أن رأس TAR ليس فارغًا                          */
        /* ========================================================= */

        int all_zero = 1;

        for (int i = 0; i < TAR_BLOCK_SIZE; i++)
        {
            if (p[i] != 0)
            {
                all_zero = 0;
                break;
            }
        }

        /* رأس صفري = نهاية الأرشيف */
        if (all_zero)
            break;


        /* ========================================================= */
        /* قراءة معلومات الملف                                      */
        /* ========================================================= */

        char* name =
            (char*)(p + TAR_OFF_NAME);

        char typeflag =
            (char)p[TAR_OFF_TYPEFLAG];

        uint32_t filesize =
            oct_to_uint(
                (char*)(p + TAR_OFF_SIZE),
                TAR_LEN_SIZE
            );


        /* ========================================================= */
        /* التحقق من أن حجم الملف لا يتجاوز الأرشيف                  */
        /* ========================================================= */

        uint32_t data_blocks =
            (filesize + TAR_BLOCK_SIZE - 1) /
            TAR_BLOCK_SIZE;

        uint32_t data_bytes =
            data_blocks * TAR_BLOCK_SIZE;

        uint8_t* data =
            p + TAR_BLOCK_SIZE;


        /*
         * التأكد من أن بداية البيانات داخل الأرشيف
         */
        if (data > end)
            break;


        /*
         * التأكد من أن بيانات الملف كاملة داخل الأرشيف.
         *
         * نستخدم:
         *
         * data_bytes > (end - data)
         *
         * بدل:
         *
         * data + data_bytes > end
         *
         * لتجنب overflow.
         */
        if (data_bytes > (uint32_t)(end - data))
        {

            break;
        }


        /* ========================================================= */
        /* ملفات عادية فقط                                          */
        /* ========================================================= */

        if (
            (typeflag == '0' || typeflag == 0) &&
            name[0] != 0 &&
            file_count < MAX_ARCHIVE_FILES
        )
        {
            file_t* f =
                &files[file_count];


            /* ===================================================== */
            /* نسخ اسم الملف بأمان                                  */
            /* ===================================================== */

            int i = 0;

            /*
             * مهم:
             *
             * لا نستخدم name[i] قبل التأكد من أن i < TAR_LEN_NAME.
             *
             * لأن حقل name في TAR حجمه 100 بايت فقط.
             */
            while (
                i < TAR_LEN_NAME &&
                i < (int)sizeof(f->name) - 1 &&
                name[i] != '\0'
            )
            {
                f->name[i] =
                    name[i];

                i++;
            }

            f->name[i] = '\0';


            /* ===================================================== */
            /* التأكد من أن الاسم ليس فارغًا بعد النسخ              */
            /* ===================================================== */

            if (f->name[0] != '\0')
            {
                f->data =
                    data;

                f->size =
                    filesize;

                file_count++;
            }
        }


        /* ========================================================= */
        /* الانتقال إلى رأس الملف التالي                             */
        /* ========================================================= */

        p =
            data + data_bytes;
    }
}

/* تعرض في الطرفية كل ما تم استخراجه من الأرشيف */
void عرض_محتويات_الأرشيف(void) {


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

    if (file_count == 0) {
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
    int color = 0;
    while (1) {
        color = (color + 1) % 0xFFFFFF;
        video_api.clear(color);
        video_api.swap();
        for (volatile int i = 0; i < 5000000; i++);  /* تأخير بسيط */
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


    panic_page_fault(
        addr,
        stack
    );
}


void task_entry(void)
{
    while (1)
    {
        if (ticks == 0)
            continue;

        /*
         * هنا لا نرسم أي شيء.
         * فقط نعمل swap لما هو موجود في back buffer.
         */
        video_api.swap();

        uint64_t last = ticks;

        while (ticks == last)
            asm volatile("hlt");
    }
}


/* ================= KERNEL MAIN ================= */
 
void KernelMain(boot_info_t *boot){
 
    uint8_t* الوسيط_start = 0;
    uint32_t الوسيط_size  = 0;
 
    core_init();
    تهيئة_الفيديو(boot);
    if (boot->system_archive && boot->system_archive_size)
    {
        parse_الوسيط(
            (uint8_t *)boot->system_archive,
            boot->system_archive_size
        );
    }
    else
    {
    }

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



    core_memory();
    core_interrupt();

    test_erorr_main();

    idle_p = process_api.create(idle);
    //scheduler_api.add(idle_p);
 
    process_t* p = process_api.create(task_entry);
    scheduler_api.add(p);
    //video_api.clear(0x00224488);
    //video_api.swap();

    loader_app(".\\النظام\\المشغل.تطبيق");

    development_screen_init();
/*
    development_screen_println("Kernel started");
    development_screen_println("Initializing video..."); // للطابعة من شاشة التطوير
    development_screen_println("Video initialized");
    development_screen_println("Loading kernel...");

    development_screen_update();
*/
    core_run();
 
    while (1)
    {
        asm volatile("hlt");
    }
}
