#include "قيم_الملف.h"
#include "محمل_التطبيقات.h"
#include "القلب/الوحدات/اللغة/لغة.h"
#include "محرك_العمليات/بوابة_العمليات.h"


extern file_t* find_file(const char* name);

/* ================================================================ */
/* توحيد مسار التطبيق                                                */
/* ================================================================ */

static int normalize_path(
    const char *source,
    char *destination,
    int size
)
{
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



static void arp_process_entry(void* arg)
{
    file_t* f = (file_t*)arg;

    if (!f)
    {
    }
    else if (!arp_api.run_script)
    {
    }
    else
    {
        int result =
            arp_api.run_script(
                (const char*)f->data,
                f->size
            );

        if (result < 0)
        {
        }
        else
        {
        }
    }

    /*
     * لا يوجد حالياً آلية لإنهاء/إزالة عملية من الجدولة،
     * لذا تبقى العملية متوقفة هنا بلا حِمل حتى تُضاف تلك الآلية.
     */
    while (1)
    {
        __asm__ volatile ("hlt");
    }
}


/* ================================================================ */
/* تشغيل سكربت ARP داخل عملية مستقلة                                  */
/* ================================================================ */

int loder_start_script(
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

    process_t *p =
        process_api.create_ex(
            arp_process_entry,
            (void*)f
        );

    if (!p)
    {
        return -1;
    }

    process_api.start(p);

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
