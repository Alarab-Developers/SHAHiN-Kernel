#include "الدوال.h"

#include "محرك_العمليات/محرك_العمليات.h"
#include "محرك_العمليات/بوابة_العمليات.h"


#define ARP_CONTEXT_SLOTS 4


static arp_function_t functions[
    ARP_CONTEXT_SLOTS
][
    ARP_MAX_FUNCTIONS
];

static int function_count[
    ARP_CONTEXT_SLOTS
];


/* ================================================================ */
/* تحديد فتحة العملية الحالية                                        */
/* ================================================================ */

static int current_slot(void)
{
    process_t *current =
        process_api.current();

    /*
     * لا توجد عملية حالية معروفة (مثلاً استدعاء مبكر جداً
     * قبل تفعيل الجدولة) → نستخدم الفتحة 0 كقيمة افتراضية آمنة.
     */
    if (!current)
        return 0;

    return
        (int)(
            ((uint64_t)current->pid) %
            ARP_CONTEXT_SLOTS
        );
}


/* ================================================================ */
/* التهيئة                                                           */
/* ================================================================ */

void functions_init(void)
{
    function_count[current_slot()] = 0;
}


/* ================================================================ */
/* البحث                                                              */
/* ================================================================ */

arp_function_t *function_find(
    const char *name
)
{
    if (!name)
        return 0;

    int slot =
        current_slot();

    for (int i = 0; i < function_count[slot]; i++)
    {
        if (
            strcmp(
                functions[slot][i].name,
                name
            ) == 0
        )
        {
            return &functions[slot][i];
        }
    }

    return 0;
}


/* ================================================================ */
/* التسجيل                                                            */
/* ================================================================ */

int function_register(
    const char *name,
    const char parameters[][64],
    int parameter_count,
    const char **lines,
    int line_count
)
{
    if (!name || !lines)
        return -1;


    if (
        parameter_count < 0 ||
        parameter_count > ARP_MAX_PARAMETERS
    )
    {
        return -1;
    }


    int slot =
        current_slot();


    if (function_count[slot] >= ARP_MAX_FUNCTIONS)
        return -1;


    if (
        line_count <= 0 ||
        line_count > ARP_MAX_FUNCTION_LINES
    )
    {
        return -1;
    }


    if (function_find(name))
    {
        return -1;
    }


    arp_function_t *f =
        &functions[slot][function_count[slot]];


    /*
     * اسم الدالة
     */
    strcpy(
        f->name,
        name
    );


    /*
     * المعاملات
     */
    f->parameter_count =
        parameter_count;


    for (
        int i = 0;
        i < parameter_count;
        i++
    )
    {
        strcpy(
            f->parameters[i],
            parameters[i]
        );
    }


    /*
     * جسم الدالة
     */
    f->line_count =
        line_count;


    for (
        int i = 0;
        i < line_count;
        i++
    )
    {
        if (!lines[i])
            return -1;


        strcpy(
            f->lines[i],
            lines[i]
        );
    }


    function_count[slot]++;

    return 0;
}
