#include "رياضيات.h"
#include "../../ادوات/ادوات.h"
#include "المتغيرات.h"
#include "الدوال.h"
#include "../المحلل.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"


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
/* البحث عن القوس المقابل                                            */
/* ================================================================ */

static const char *find_matching_paren(
    const char *p
)
{
    int depth = 0;

    while (*p)
    {
        if (*p == '(')
        {
            depth++;
        }

        else if (*p == ')')
        {
            if (depth == 0)
            {
                return p;
            }

            depth--;
        }

        p++;
    }

    return NULL;
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
     * رقم ست عشري
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
     * رقم عشري
     */

    const char *end;


    double value =
        string_to_double(
            current,
            &end
        );


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
/* قراءة اسم متغير                                                    */
/* ================================================================ */

static int parse_variable(
    double *value
)
{
    skip_math_spaces();


    char name[64];

    int length = 0;


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
        *current != ',' &&
        *current != '>' &&
        *current != '<' &&
        *current != '=' &&
        *current != '!' &&
        length < 63
    )
    {
        name[length++] =
            *current++;
    }


    name[length] =
        '\0';


    if (length == 0)
    {
        return -1;
    }


    return variable_get(
        name,
        value
    );
}


/* ================================================================ */
/* العامل الأساسي                                                    */
/* رقم / متغير / دالة / (تعبير)                                    */
/* ================================================================ */

static double parse_factor(
    int *error
)
{
    skip_math_spaces();


    /*
     * ============================================================
     * ( تعبير )
     * ============================================================
     */

    if (*current == '(')
    {
        current++;


        double value =
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
     * ============================================================
     * سالب
     * ============================================================
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
     * ============================================================
     * رقم
     * ============================================================
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
     * ============================================================
     * قراءة اسم
     *
     * يمكن أن يكون:
     *
     * متغير
     *
     * أو دالة
     *
     * مثال:
     *
     * س
     *
     * اختبار(3,2)
     * ============================================================
     */

    char name[64];

    int name_len = 0;


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
        *current != ',' &&
        *current != '>' &&
        *current != '<' &&
        *current != '=' &&
        *current != '!' &&
        name_len < 63
    )
    {
        name[name_len++] =
            *current++;
    }


    name[name_len] =
        '\0';


    if (name_len == 0)
    {
        *error = 1;

        return 0;
    }


    skip_math_spaces();


    /*
     * ============================================================
     * استدعاء دالة
     * ============================================================
     */

    if (*current == '(')
    {
        const char *func_open =
            current;

        const char *args_start =
            current + 1;


        /*
         * إيجاد )
         */

        const char *close =
            find_matching_paren(
                args_start
            );


        if (!close)
        {
            *error = 1;

            return 0;
        }


        /*
         * الانتقال بعد ) مباشرة، ثم استدعاء الدالة عبر
         * المسار المشترك (يتولى قراءة المعاملات، وربطها،
         * وتشغيل جسم الدالة)
         */

        current =
            close + 1;


        int result =
            arp_invoke_function(
                name,
                args_start
            );


        /*
         * الدالة أعادت قيمة
         */

        if (result == -4)
        {
            /*
             * لا يمكن استخدام قيمة نصية داخل تعبير حسابي
             */

            if (arp_return_type == ARP_RETURN_TEXT)
            {
                *error = 1;

                return 0;
            }

            return arp_return_value;
        }


        /*
         * خطأ أثناء التنفيذ، أو دالة بدون ارجع
         */

        *error = 1;

        return 0;
    }


    /*
     * ============================================================
     * متغير
     * ============================================================
     */

    double value;


    if (
        variable_get(
            name,
            &value
        ) == 0
    )
    {
        return value;
    }


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


            value /=
                divisor;
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


    if (*current != '\0')
    {
        *error = 1;
    }


    return result;
}
