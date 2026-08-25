#include "مكتبات_اللغة.h"

#include "../../../لغة.h"

#include "القلب/الوحدات/محمل_التطبيقات/قيم_الملف.h"



extern file_t* find_file(
    const char *name
);

static void normalize_path(
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
        size <= 0
    )
    {
        return;
    }


    /*
     * تجاهل الجذر:
     *
     * .\
     *
     * أو:
     *
     * ./
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


    /*
     * نسخ المسار وتوحيد الفاصل
     */
    while (
        source[i] &&
        j < size - 1
    )
    {
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
}


/* ================================================================ */
/* التحقق من امتداد المكتبة                                          */
/* ================================================================ */

static int is_arp_library(
    const char *name
)
{
    if (!name)
        return 0;


    const char *dot = 0;


    for (
        const char *p = name;
        *p;
        p++
    )
    {
        if (*p == '.')
        {
            dot = p;
        }
    }


    if (!dot)
        return 0;


    return
        strcmp(
            dot,
            ".تطبيق"
        ) == 0;
}


/* ================================================================ */
/* استدعاء مكتبة                                                      */
/* ================================================================ */

int library_import(
    const char *name
)
{
    if (
        !name ||
        !name[0]
    )
    {
        return -1;
    }


    /*
     * يجب أن تكون مكتبة ARP
     */
    if (
        !is_arp_library(
            name
        )
    )
    {
        return -1;
    }

    char path[256];

    normalize_path(
        name,
        path,
        sizeof(path)
    );

    /*
     * البحث عن الملف
     */
    file_t *f =
        find_file(
            path
        );


    if (!f)
    {
        return -1;
    }


    /*
     * التحقق من وجود بيانات
     */
    if (
        !f->data ||
        f->size == 0
    )
    {

        return -1;
    }


    /*
     * تشغيل المكتبة
     *
     * بدون مسح الدوال الموجودة
     */
    return
        arp_run_library(
            (const char *)f->data,
            f->size
        );
}
