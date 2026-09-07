#include "المتغيرات.h"
#include "القلب/الوحدات/محمل_التطبيقات/سياق_اللغة.h"
#include "محرك_العمليات/محرك_العمليات.h"
#include "محرك_العمليات/بوابة_العمليات.h"
#include "../../ادوات/ادوات.h"


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
/* نسخ نص                                                             */
/* ================================================================ */

static void copy_text(
    char *destination,
    const char *source,
    int destination_size
)
{
    int i = 0;


    if (
        !destination ||
        !source ||
        destination_size <= 0
    )
    {
        return;
    }


    while (
        source[i] &&
        i < destination_size - 1
    )
    {
        destination[i] =
            source[i];

        i++;
    }


    destination[i] = '\0';
}


/* ================================================================ */
/* اكتشاف نوع الرقم تلقائياً: صحيح أم عائم                            */
/*                                                                    */
/* - إن لم يكن للقيمة جزء عشري: NUMBER_KIND_INT، وتُحفظ كما هي.       */
/* - إن كان لها جزء عشري: NUMBER_KIND_FLOAT، وتُمرَّر عبر float       */
/*   (32-بت) ثم تُعاد إلى double، فتفقد بالضبط نفس الدقة التي كانت    */
/*   ستفقدها لو خُزِّنت في متغير float حقيقي بلغة C.                  */
/* ================================================================ */

static number_kind_t detect_number_kind(
    double value,
    double *stored_value
)
{
    double magnitude =
        (value < 0) ? -value : value;

    /*
     * قيم أكبر من مدى int64 لا يمكن اختبار "هل هي صحيحة" بأمان
     * عبر التحويل إليه، فنعاملها دائماً كعائمة
     */
    if (magnitude < 9223372036854775000.0)
    {
        if (value == (double)(int64_t)value)
        {
            *stored_value = value;
            return NUMBER_KIND_INT;
        }
    }

    float reduced = (float)value;

    *stored_value = (double)reduced;

    return NUMBER_KIND_FLOAT;
}


/* ================================================================ */
/* تهيئة المتغيرات                                                   */
/* ================================================================ */

void variables_init(void)
{
    arp_context_t* ctx =
        arp_get_current_context();

    if (!ctx)
        return;


    for (
        int i = 0;
        i < MAX_VARIABLES;
        i++
    )
    {
        ctx->variables[i].used =
            0;

        ctx->variables[i].name[0] =
            '\0';

        ctx->variables[i].value =
            0;

        ctx->variables[i].text[0] =
            '\0';

        ctx->variables[i].type =
            VARIABLE_NUMBER;

        ctx->variables[i].number_kind =
            NUMBER_KIND_INT;
    }
}


/* ================================================================ */
/* إنشاء أو تعديل متغير رقمي                                        */
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


    arp_context_t* ctx =
        arp_get_current_context();

    if (!ctx)
        return -1;


    double stored_value = value;

    number_kind_t kind =
        detect_number_kind(
            value,
            &stored_value
        );


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
            ctx->variables[i].used &&
            text_equal(
                ctx->variables[i].name,
                name
            )
        )
        {
            ctx->variables[i].value =
                stored_value;

            ctx->variables[i].text[0] =
                '\0';

            ctx->variables[i].type =
                VARIABLE_NUMBER;

            ctx->variables[i].number_kind =
                kind;

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
        if (!ctx->variables[i].used)
        {
            copy_name(
                ctx->variables[i].name,
                name
            );

            ctx->variables[i].value =
                stored_value;

            ctx->variables[i].text[0] =
                '\0';

            ctx->variables[i].type =
                VARIABLE_NUMBER;

            ctx->variables[i].number_kind =
                kind;

            ctx->variables[i].used =
                1;

            return 0;
        }
    }


    return -1;
}


/* ================================================================ */
/* قراءة متغير رقمي                                                  */
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


    arp_context_t* ctx =
        arp_get_current_context();

    if (!ctx)
        return -1;


    for (
        int i = 0;
        i < MAX_VARIABLES;
        i++
    )
    {
        if (
            ctx->variables[i].used &&
            text_equal(
                ctx->variables[i].name,
                name
            )
        )
        {
            /*
             * المتغير نصي وليس رقميًا
             */
            if (
                ctx->variables[i].type !=
                VARIABLE_NUMBER
            )
            {
                return -1;
            }


            *value =
                ctx->variables[i].value;

            return 0;
        }
    }


    return -1;
}


/* ================================================================ */
/* معرفة نوع متغير رقمي: صحيح أم عائم                                 */
/* ================================================================ */

int variable_get_kind(
    const char *name,
    number_kind_t *kind
)
{
    if (
        !name ||
        !kind
    )
    {
        return -1;
    }


    arp_context_t* ctx =
        arp_get_current_context();

    if (!ctx)
        return -1;


    for (
        int i = 0;
        i < MAX_VARIABLES;
        i++
    )
    {
        if (
            ctx->variables[i].used &&
            text_equal(
                ctx->variables[i].name,
                name
            )
        )
        {
            if (
                ctx->variables[i].type !=
                VARIABLE_NUMBER
            )
            {
                return -1;
            }


            *kind =
                ctx->variables[i].number_kind;

            return 0;
        }
    }


    return -1;
}


/* ================================================================ */
/* تحويل متغير (رقمي أو نصي) إلى نص جاهز للطباعة                     */
/* ================================================================ */

int variable_to_string(
    const char *name,
    char *buffer,
    int buffer_size
)
{
    if (
        !name ||
        !buffer ||
        buffer_size <= 0
    )
    {
        return -1;
    }


    arp_context_t* ctx =
        arp_get_current_context();

    if (!ctx)
        return -1;


    for (
        int i = 0;
        i < MAX_VARIABLES;
        i++
    )
    {
        if (
            ctx->variables[i].used &&
            text_equal(
                ctx->variables[i].name,
                name
            )
        )
        {
            if (
                ctx->variables[i].type ==
                VARIABLE_TEXT
            )
            {
                copy_text(
                    buffer,
                    ctx->variables[i].text,
                    buffer_size
                );

                return 0;
            }


            /*
             * صحيح -> بدون فاصلة عشرية (precision = 0)
             * عائم  -> بدقة قريبة من float في C (precision = 6)
             */
            int precision =
                (ctx->variables[i].number_kind == NUMBER_KIND_INT)
                    ? 0
                    : 6;

            char temp[64];

            double_to_string(
                ctx->variables[i].value,
                temp,
                precision
            );

            copy_text(
                buffer,
                temp,
                buffer_size
            );

            return 0;
        }
    }


    return -1;
}


/* ================================================================ */
/* إنشاء أو تعديل متغير نصي                                         */
/* ================================================================ */

int variable_set_text(
    const char *name,
    const char *text
)
{
    if (
        !name ||
        !*name ||
        !text
    )
    {
        return -1;
    }


    arp_context_t* ctx =
        arp_get_current_context();

    if (!ctx)
        return -1;


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
            ctx->variables[i].used &&
            text_equal(
                ctx->variables[i].name,
                name
            )
        )
        {
            copy_text(
                ctx->variables[i].text,
                text,
                MAX_VARIABLE_TEXT
            );

            ctx->variables[i].value =
                0;

            ctx->variables[i].type =
                VARIABLE_TEXT;

            return 0;
        }
    }


    /*
     * إنشاء متغير نصي جديد
     */
    for (
        int i = 0;
        i < MAX_VARIABLES;
        i++
    )
    {
        if (!ctx->variables[i].used)
        {
            copy_name(
                ctx->variables[i].name,
                name
            );

            ctx->variables[i].value =
                0;

            copy_text(
                ctx->variables[i].text,
                text,
                MAX_VARIABLE_TEXT
            );

            ctx->variables[i].type =
                VARIABLE_TEXT;

            ctx->variables[i].used =
                1;

            return 0;
        }
    }


    return -1;
}


/* ================================================================ */
/* قراءة متغير نصي                                                   */
/* ================================================================ */

int variable_get_text(
    const char *name,
    char *text,
    int text_size
)
{
    if (
        !name ||
        !text ||
        text_size <= 0
    )
    {
        return -1;
    }


    arp_context_t* ctx =
        arp_get_current_context();

    if (!ctx)
        return -1;


    for (
        int i = 0;
        i < MAX_VARIABLES;
        i++
    )
    {
        if (
            ctx->variables[i].used &&
            text_equal(
                ctx->variables[i].name,
                name
            )
        )
        {
            /*
             * المتغير رقمي وليس نصيًا
             */
            if (
                ctx->variables[i].type !=
                VARIABLE_TEXT
            )
            {
                return -1;
            }


            copy_text(
                text,
                ctx->variables[i].text,
                text_size
            );

            return 0;
        }
    }


    return -1;
}
