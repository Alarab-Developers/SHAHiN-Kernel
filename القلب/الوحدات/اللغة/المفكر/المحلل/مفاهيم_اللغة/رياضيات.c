#include "رياضيات.h"
#include "../../../ادوات/ادوات.h"
#include "المتغيرات.h"


static const char *current;


/* ================================================================ */
/* تخطي المسافات                                                     */
/* ================================================================ */

static void skip_math_spaces(void)
{
    while (
        *current == ' ' ||
        *current == '\t'
    )
    {
        current++;
    }
}


/* ================================================================ */
/* قراءة رقم                                                         */
/* ================================================================ */

static double parse_number(
    int *error
)
{
    skip_math_spaces();


    /*
     * ============================================================
     * رقم ست عشري
     *
     * مثال:
     *
     * 0x00224488
     * 0XFF0000FF
     * ============================================================
     */

    if (
        current[0] == '0' &&
        (
            current[1] == 'x' ||
            current[1] == 'X'
        )
    )
    {
        current += 2;


        uint32_t value = 0;

        int digits = 0;


        while (*current)
        {
            char c =
                *current;

            uint32_t digit;


            if (
                c >= '0' &&
                c <= '9'
            )
            {
                digit =
                    (uint32_t)(c - '0');
            }
            else if (
                c >= 'A' &&
                c <= 'F'
            )
            {
                digit =
                    (uint32_t)(c - 'A' + 10);
            }
            else if (
                c >= 'a' &&
                c <= 'f'
            )
            {
                digit =
                    (uint32_t)(c - 'a' + 10);
            }
            else
            {
                break;
            }


            value =
                (value << 4) |
                digit;

            digits++;

            current++;
        }


        if (digits == 0)
        {
            *error = 1;

            return 0;
        }


        return (double)value;
    }


    /*
     * ============================================================
     * رقم عشري عادي
     *
     * مثال:
     *
     * 100
     * 25.5
     * 3.14
     * ============================================================
     */

    const char *end;


    double value =
        string_to_double(
            current,
            &end
        );


    /*
     * لم يتم قراءة أي رقم
     */
    if (end == current)
    {
        *error = 1;

        return 0;
    }


    current =
        end;


    return value;
}

/* ================================================================ */
/* قراءة اسم متغير                                                   */
/* ================================================================ */

static int parse_variable(
    double *value
)
{
    skip_math_spaces();


    char name[64];

    int length = 0;


    /*
     * قراءة الاسم حتى الوصول إلى:
     *
     * مسافة
     * +
     * -
     * *
     * /
     * (
     * )
     */
    while (
        *current &&
        *current != ' ' &&
        *current != '\t' &&
        *current != '+' &&
        *current != '-' &&
        *current != '*' &&
        *current != '/' &&
        *current != '(' &&
        *current != ')' &&
        length < 63
    )
    {
        name[length++] =
            *current++;
    }


    name[length] = '\0';


    if (length == 0)
        return -1;


    return variable_get(
        name,
        value
    );
}



/* ================================================================ */
/* العامل الأساسي                                                    */
/* الرقم أو (معادلة)                                                 */
/* ================================================================ */

static double parse_factor(
    int *error
)
{
    skip_math_spaces();

    if (*current == '(')
    {
        current++;

        double value;

        value =
            0;


        /* سيتم استدعاء تحليل الجمع هنا */
        extern double math_parse_expression(
            int *error
        );

        value =
            math_parse_expression(
                error
            );

        skip_math_spaces();

        if (*current != ')')
        {
            *error = 1;

            return 0;
        }

        current++;

        return value;
    }


    /*
     * سالب
     */
    if (*current == '-')
    {
        current++;

        return -
            parse_factor(
                error
            );
    }


    /*
     * إذا كان أول حرف رقمًا
     */
    if (
        (*current >= '0' && *current <= '9') ||
        *current == '.'
    )
    {
        return
            parse_number(
                error
            );
    }


    /*
     * محاولة قراءة متغير
     */
    double value;


    if (
        parse_variable(
            &value
        ) == 0
    )
    {
        return value;
    }


    /*
     * متغير غير موجود
     */
    *error = 1;

    return 0;
}


/* ================================================================ */
/* الضرب والقسمة                                                     */
/* ================================================================ */

static double parse_term(
    int *error
)
{
    double value =
        parse_factor(
            error
        );

    while (!*error)
    {
        skip_math_spaces();

        if (*current == '*')
        {
            current++;

            value *=
                parse_factor(
                    error
                );
        }

        else if (*current == '/')
        {
            current++;

            double divisor =
                parse_factor(
                    error
                );

            if (divisor == 0)
            {
                *error = 1;

                return 0;
            }

            value /= divisor;
        }

        else
        {
            break;
        }
    }

    return value;
}


/* ================================================================ */
/* الجمع والطرح                                                      */
/* ================================================================ */

double math_parse_expression(
    int *error
)
{
    double value =
        parse_term(
            error
        );

    while (!*error)
    {
        skip_math_spaces();

        if (*current == '+')
        {
            current++;

            value +=
                parse_term(
                    error
                );
        }

        else if (*current == '-')
        {
            current++;

            value -=
                parse_term(
                    error
                );
        }

        else
        {
            break;
        }
    }

    return value;
}


/* ================================================================ */
/* البوابة الرئيسية للمدير الرياضي                                   */
/* ================================================================ */

double math_calculate(
    const char *expression,
    int *error
)
{
    if (
        !expression ||
        !error
    )
    {
        return 0;
    }

    *error = 0;

    current =
        expression;


    double result =
        math_parse_expression(
            error
        );


    skip_math_spaces();


    /*
     * وجود رموز غير مفهومة
     */
    if (*current != '\0')
    {
        *error = 1;
    }

    return result;
}
