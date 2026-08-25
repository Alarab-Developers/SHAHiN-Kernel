#include "المتغيرات.h"

#include "محرك_العمليات/محرك_العمليات.h"
#include "محرك_العمليات/بوابة_العمليات.h"



#define ARP_CONTEXT_SLOTS 4


static variable_t variables[
    ARP_CONTEXT_SLOTS
][
    MAX_VARIABLES
];


/* ================================================================ */
/* تحديد فتحة العملية الحالية                                        */
/* ================================================================ */

static int current_slot(void)
{
    process_t *current =
        process_api.current();

    if (!current)
        return 0;

    return
        (int)(
            ((uint64_t)current->pid) %
            ARP_CONTEXT_SLOTS
        );
}


/* ================================================================ */
/* مقارنة نصين                                                       */
/* ================================================================ */

static int text_equal(
    const char *a,
    const char *b
)
{
    if (!a || !b)
        return 0;


    while (*a && *b)
    {
        if (*a != *b)
            return 0;

        a++;
        b++;
    }


    return
        *a == '\0' &&
        *b == '\0';
}


/* ================================================================ */
/* نسخ اسم                                                           */
/* ================================================================ */

static void copy_name(
    char *destination,
    const char *source
)
{
    int i = 0;


    while (
        source[i] &&
        i < MAX_VARIABLE_NAME - 1
    )
    {
        destination[i] =
            source[i];

        i++;
    }


    destination[i] = '\0';
}


/* ================================================================ */
/* تهيئة المتغيرات                                                   */
/* ================================================================ */

void variables_init(void)
{
    int slot =
        current_slot();

    for (
        int i = 0;
        i < MAX_VARIABLES;
        i++
    )
    {
        variables[slot][i].used = 0;

        variables[slot][i].name[0] = '\0';

        variables[slot][i].value = 0;
    }
}


/* ================================================================ */
/* إنشاء أو تعديل متغير                                              */
/* ================================================================ */

int variable_set(
    const char *name,
    double value
)
{
    if (
        !name ||
        !*name
    )
    {
        return -1;
    }


    int slot =
        current_slot();


    /*
     * البحث عن متغير موجود
     */
    for (
        int i = 0;
        i < MAX_VARIABLES;
        i++
    )
    {
        if (
            variables[slot][i].used &&
            text_equal(
                variables[slot][i].name,
                name
            )
        )
        {
            variables[slot][i].value =
                value;

            return 0;
        }
    }


    /*
     * إنشاء متغير جديد
     */
    for (
        int i = 0;
        i < MAX_VARIABLES;
        i++
    )
    {
        if (!variables[slot][i].used)
        {
            copy_name(
                variables[slot][i].name,
                name
            );

            variables[slot][i].value =
                value;

            variables[slot][i].used =
                1;

            return 0;
        }
    }


    /*
     * جدول المتغيرات ممتلئ
     */
    return -1;
}


/* ================================================================ */
/* قراءة قيمة متغير                                                  */
/* ================================================================ */

int variable_get(
    const char *name,
    double *value
)
{
    if (
        !name ||
        !value
    )
    {
        return -1;
    }


    int slot =
        current_slot();


    for (
        int i = 0;
        i < MAX_VARIABLES;
        i++
    )
    {
        if (
            variables[slot][i].used &&
            text_equal(
                variables[slot][i].name,
                name
            )
        )
        {
            *value =
                variables[slot][i].value;

            return 0;
        }
    }


    return -1;
}
