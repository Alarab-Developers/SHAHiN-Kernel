#include "قيم_الملف.h"
#include "محمل_التطبيقات.h"
#include "القلب/الوحدات/اللغة/لغة.h"
#include "محرك_العمليات/بوابة_العمليات.h"
#include "سياق_اللغة.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "القلب/الوحدات/شاشة_التطوير/شاشة_التطوير.h"

extern file_t* find_file(const char* name);

/* ================================================================ */
/* توحيد مسار التطبيق                                                */
/* ================================================================ */

static int normalize_path(const char *source, char *destination, int size){

    int i = 0;
    int j = 0;


    if (
        !source ||
        !destination ||
        size <= 1
    )
    {
        return -1;
    }


    /*
     * تجاهل .\ أو ./
     *
     * مثال:
     *
     * .\المكتبات\برنامج.تطبيق
     *
     * يصبح:
     *
     * المكتبات/برنامج.تطبيق
     */
    if (
        source[0] == '.' &&
        (
            source[1] == '\\' ||
            source[1] == '/'
        )
    )
    {
        i = 2;
    }


    while (
        source[i] &&
        j < size - 1
    )
    {
        /*
         * توحيد \ إلى /
         */
        if (source[i] == '\\')
        {
            destination[j] = '/';
        }
        else
        {
            destination[j] = source[i];
        }

        i++;
        j++;
    }


    destination[j] = '\0';


    return 0;
}

/* ================================================================ */
/* التحقق من امتداد الملف                                           */
/* ================================================================ */

static int is_arp_file(const char* name)
{
    if (!name)
        return 0;

    const char* dot = 0;

    for (const char* p = name; *p; p++)
    {
        if (*p == '.')
            dot = p;
    }

    if (!dot)
        return 0;

    return strcmp(dot, ".تطبيق") == 0;
}


/* ================================================================ */
/* تشغيل سكربت ARP                                                   */
/* ================================================================ */

int loder_run_script(
    const char *name
)
{
    if (!name || !name[0])
        return -1;

    if (!is_arp_file(name))
        return -1;

    char path[256];


    if (
        normalize_path(
            name,
            path,
            sizeof(path)
        ) < 0
    )
    {
        return -1;
    }


    file_t *f =
        find_file(
            path
        );

    if (!f)
    {
        return -1;
    }

    if (
        !arp_api.run_script
    )
    {

        return -1;
    }

    return arp_api.run_script(
        (const char *)f->data,
        f->size
    );
}



static void arp_process_entry(void* arg) {
    file_t* f = (file_t*)arg;

    if (f && arp_api.run_script) {
        arp_api.run_script((const char*)f->data, f->size);
    }

    // إنهاء العملية الحالية عبر البوابة
    process_api.exit();

    while (1) asm volatile("hlt");
}


/* ================================================================ */
/* تشغيل سكربت ARP داخل عملية مستقلة                                  */
/* ================================================================ */

int loder_start_script(
    const char *name
)
{

    //development_screen_println(
   //     "LOADER: start"
   // );

   // development_screen_update();

    //development_screen_println(
    //    "LOADER: new application request"
   // );

   // development_screen_update();

    if (!name || !name[0])
        return -1;

    if (!is_arp_file(name))
        return -1;

    char path[256];

    if (
        normalize_path(
            name,
            path,
            sizeof(path)
        ) < 0
    )
    {
        return -1;
    }

    file_t *f =
        find_file(path);

    if (!f)
    {
       // development_screen_println(
         //   "LOADER: file NOT found"
        //);

        //development_screen_update();

        return -1;
    }

//    development_screen_println(
  //      "LOADER: file found"
    //);

    //development_screen_update();

    if (!f)
        return -1;

    process_t *p =
        process_api.create_ex(
            arp_process_entry,
            (void*)f
        );

    if (!p)
    {
      //  development_screen_println(
        //    "LOADER: create_process FAILED"
        //);

        //development_screen_update();

        return -1;
    }

   // development_screen_println(
    //    "LOADER: process created"
    //);

    //development_screen_update();

    if (!p)
        return -1;

    /*
     * إنشاء سياق ARP مستقل لهذه العملية
     */
    arp_context_t *ctx =
        memory_api.alloc(
            sizeof(arp_context_t)
        );

    if (!p)
    {
      //  development_screen_println(
        //    "LOADER: create_process FAILED"
        //);

        //development_screen_update();

        return -1;
    }

   // development_screen_println(
     //   "LOADER: process created"
    //);

    //development_screen_update();

    if (!ctx)
        return -1;

    memset(
        ctx,
        0,
        sizeof(arp_context_t)
    );

    process_api.set_app_context(
        p,
        ctx
    );

   // development_screen_println(
      //  "LOADER: context attached"
    //);

    //development_screen_update();

    process_api.start(p);

    //development_screen_println(
      //  "LOADER: process STARTED"
    //);

    //development_screen_update();

    return 0;
}


/* ================================================================ */
/* تشغيل التطبيق الرئيسي                                             */
/* ================================================================ */

void loder_auto_start(void)
{



    if (
        loder_start_script(
            AUTO_APP_NAME
        ) != 0
    )
    {
    }
}


int loader_app(
    const char* name
)
{
    return loder_start_script(name);
}
