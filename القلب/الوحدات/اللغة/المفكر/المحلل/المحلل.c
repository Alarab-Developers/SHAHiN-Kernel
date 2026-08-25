#include "المحلل.h"

#include "../../لغة.h"
#include "../../ادوات/ادوات.h"
#include "../../المنفذ/المنفذ.h"
#include "مفاهيم_اللغة/الدوال.h"
#include "مفاهيم_اللغة/الحلقات.h"
#include "مفاهيم_اللغة/الشروط.h"
#include "مفاهيم_اللغة/رياضيات.h"
#include "مفاهيم_اللغة/المتغيرات.h"
#include "مفاهيم_اللغة/مكتبات_اللغة.h"
#include "اوامر_اللغة/اوامر_اللغة.h"




int arp_parse_function(
    const char *header,
    const char **body,
    int line_count
)
{
    if (!header || !body || line_count < 0)
        return -1;

    const char *p =
        skip_spaces(header);

    if (!starts_with(p, "دالة("))
        return -1;

    p =
        skip_prefix(
            p,
            "دالة("
        );

    p =
        skip_spaces(p);


    /*
     * قراءة اسم الدالة
     */
    char name[64];

    int name_length = 0;

    while (
        *p &&
        *p != ',' &&
        *p != ')' &&
        name_length < 63
    )
    {
        name[name_length++] =
            *p++;
    }

    name[name_length] =
        '\0';


    /*
     * حذف المسافات من نهاية الاسم
     */
    while (
        name_length > 0 &&
        (
            name[name_length - 1] == ' ' ||
            name[name_length - 1] == '\t'
        )
    )
    {
        name[--name_length] =
            '\0';
    }


    if (name_length == 0)
    {
        return -1;
    }


    /*
     * قراءة أسماء المعاملات
     */
    char parameters[
        ARP_MAX_PARAMETERS
    ][64];

    int parameter_count = 0;


    while (*p && *p != ')')
    {
        /*
         * يجب وجود فاصلة بعد اسم الدالة
         * أو بعد كل معامل.
         */
        if (*p == ',')
        {
            p++;

            p =
                skip_spaces(p);

            if (*p == ')')
                break;
        }
        else if (p != header)
        {
            /*
             * إذا لم تكن هذه أول قراءة
             * فيجب أن نكون بعد فاصلة.
             */
        }


        if (*p == ')')
            break;


        if (
            parameter_count >=
            ARP_MAX_PARAMETERS
        )
        {

            return -1;
        }


        int length = 0;


        while (
            *p &&
            *p != ',' &&
            *p != ')' &&
            length < 63
        )
        {
            parameters[
                parameter_count
            ][length++] =
                *p++;
        }


        parameters[
            parameter_count
        ][length] =
            '\0';


        /*
         * حذف المسافات
         */
        while (
            length > 0 &&
            (
                parameters[
                    parameter_count
                ][length - 1] == ' ' ||

                parameters[
                    parameter_count
                ][length - 1] == '\t'
            )
        )
        {
            parameters[
                parameter_count
            ][--length] =
                '\0';
        }


        if (length == 0)
        {
            return -1;
        }


        parameter_count++;


        p =
            skip_spaces(p);


        if (*p == ',')
        {
            p++;

            p =
                skip_spaces(p);

            continue;
        }


        if (*p == ')')
            break;

        return -1;
    }


    /*
     * يجب وجود نهاية )
     */
    if (*p != ')')
    {
        return -1;
    }


    p++;


    p =
        skip_spaces(p);


    /*
     * يجب وجود {
     */
    if (*p != '{')
    {

        return -1;
    }


    /*
     * التسجيل
     *
     * function_register يحتاج:
     *
     * name
     * parameters
     * parameter_count
     * lines
     * line_count
     */
    return function_register(
        name,
        parameters,
        parameter_count,
        body,
        line_count
    );
}



int parse_function_arguments(
    const char *p,
    double arguments[],
    int *argument_count
)
{
    if (
        !p ||
        !arguments ||
        !argument_count
    )
    {
        return -1;
    }


    *argument_count = 0;


    p =
        skip_spaces(
            p
        );


    /*
     * دالة بدون معاملات:
     *
     * اختبار()
     */
    if (*p == ')')
    {
        return 0;
    }


    while (*p)
    {
        if (
            *argument_count >=
            ARP_MAX_PARAMETERS
        )
        {
            return -1;
        }


        char expression[256];

        int length = 0;

        int depth = 0;


        /*
         * قراءة المعامل حتى:
         *
         * ,
         *
         * أو )
         */
        while (
            *p &&
            length < 255
        )
        {
            if (*p == '(')
            {
                depth++;
            }
            else if (*p == ')')
            {
                if (depth == 0)
                    break;

                depth--;
            }


            if (
                *p == ',' &&
                depth == 0
            )
            {
                break;
            }


            expression[length++] =
                *p++;
        }


        /*
         * حذف المسافات من النهاية
         */
        while (
            length > 0 &&
            (
                expression[length - 1] == ' ' ||
                expression[length - 1] == '\t' ||
                expression[length - 1] == '\r' ||
                expression[length - 1] == '\n'
            )
        )
        {
            length--;
        }


        expression[length] =
            '\0';


        if (length == 0)
        {
            return -1;
        }


        /*
         * حساب قيمة المعامل
         */
        int error = 0;


        double value =
            math_calculate(
                expression,
                &error
            );


        if (error)
        {
            return -1;
        }


        arguments[
            *argument_count
        ] =
            value;


        (*argument_count)++;


        p =
            skip_spaces(
                p
            );


        /*
         * نهاية المعاملات
         */
        if (*p == ')')
        {
            return 0;
        }


        /*
         * يجب وجود فاصلة
         */
        if (*p != ',')
        {
            return -1;
        }


        p++;


        p =
            skip_spaces(
                p
            );


        /*
         * منع:
         *
         * دالة(1,)
         */
        if (*p == ')')
        {
            return -1;
        }
    }


    return -1;
}




/* ================================================================ */
/* قراءة معامل رياضي                                                 */
/* ================================================================ */

int parse_numeric_argument(
    const char **p,
    double *value,
    int last
)
{
    if (
        !p ||
        !*p ||
        !value
    )
    {
        return -1;
    }


    *p =
        skip_spaces(
            *p
        );


    char expression[256];

    int length = 0;

    int depth = 0;


    while (
        **p &&
        length < 255
    )
    {
        if (**p == '(')
        {
            depth++;
        }
        else if (**p == ')')
        {
            if (depth == 0)
                break;

            depth--;
        }


        if (
            !last &&
            **p == ',' &&
            depth == 0
        )
        {
            break;
        }


        expression[length++] =
            **p;

        (*p)++;
    }


    /*
     * حذف المسافات من النهاية
     */
    while (
        length > 0 &&
        (
            expression[length - 1] == ' ' ||
            expression[length - 1] == '\t' ||
            expression[length - 1] == '\r' ||
            expression[length - 1] == '\n'
        )
    )
    {
        length--;
    }


    expression[length] =
        '\0';


    if (length == 0)
    {
        return -1;
    }


    int error = 0;


    *value =
        math_calculate(
            expression,
            &error
        );


    if (error)
    {
        return -1;
    }


    *p =
        skip_spaces(
            *p
        );


    if (!last)
    {
        if (**p != ',')
        {
            return -1;
        }

        (*p)++;
    }


    return 0;
}


/* ================================================================ */
/* تنفيذ مجموعة أسطر                                                 */
/* ================================================================ */

int arp_execute_lines(
    const char lines[][ARP_MAX_LINE_LENGTH],
    int line_count
)
{
    if (!lines || line_count < 0)
        return -1;


    int i = 0;


    while (i < line_count)
    {
        const char *line =
            lines[i];


        const char *p =
            skip_spaces(line);


        /*
         * سطر فارغ
         */
        if (*p == '\0')
        {
            i++;
            continue;
        }

        /* ======================================================== */
        /* بداية حلقة                                                 */
        /* ======================================================== */

        if (
            loop_is_start(
                p
            )
        )
        {
            int loop_end =
                -1;


            int depth =
                1;


            /*
             * البحث عن نهاية_الحلقة
             *
             * مع دعم الحلقات المتداخلة.
             */
            for (
                int j = i + 1;
                j < line_count;
                j++
            )
            {
                const char *b =
                    skip_spaces(
                        lines[j]
                    );


                /*
                 * حلقة متداخلة
                 */
                if (
                    loop_is_start(
                        b
                    )
                )
                {
                    depth++;

                    continue;
                }


                /*
                 * نهاية حلقة
                 */
                if (
                    loop_is_end(
                        b
                    )
                )
                {
                    depth--;


                    if (depth == 0)
                    {
                        loop_end =
                            j;

                        break;
                    }
                }
            }


            /*
             * لم نجد نهاية_الحلقة
             */
            if (loop_end < 0)
            {
                return -1;
            }


            /*
             * بداية جسم الحلقة
             */
            int body_start =
                i + 1;


            /*
             * عدد أسطر جسم الحلقة
             */
            int body_count =
                loop_end -
                body_start;


            /*
             * تنفيذ الحلقة
             */
            if (
                loop_execute(
                    &lines[body_start],
                    body_count
                ) < 0
            )
            {
                return -1;
            }


            /*
             * هذا السطر لن يتم الوصول إليه
             * في الحلقة اللانهائية حالياً،
             * لكنه مهم إذا أضفت لاحقاً شرط خروج.
             */
            i =
                loop_end + 1;

            continue;
        }


        /* ======================================================== */
        /* بداية شرط                                                  */
        /* ======================================================== */

        if (
            condition_is_start(
                p
            )
        )
        {
            int condition_start =
                i;

            int condition_end =
                -1;

            int else_line =
                -1;

            int depth =
                1;


            /*
             * البحث عن:
             *
             * والا
             *
             * و:
             *
             * نهاية_الشرط
             *
             * مع دعم الشروط المتداخلة.
             */
            for (
                int j = i + 1;
                j < line_count;
                j++
            )
            {
                const char *b =
                    skip_spaces(
                        lines[j]
                    );


                /*
                 * شرط متداخل
                 */
                if (
                    condition_is_start(
                        b
                    )
                )
                {
                    depth++;
                    continue;
                }


                /*
                 * نهاية شرط
                 */
                if (
                    condition_is_end(
                        b
                    )
                )
                {
                    depth--;

                    if (depth == 0)
                    {
                        condition_end =
                            j;

                        break;
                    }

                    continue;
                }


                /*
                 * والا
                 *
                 * نقبلها فقط في مستوى الشرط الحالي،
                 * وليس داخل شرط متداخل.
                 */
                if (
                    condition_is_else(
                        b
                    )
                )
                {
                    if (depth == 1)
                    {
                        /*
                         * منع وجود والا مرتين
                         */
                        if (else_line >= 0)
                        {
                            return -1;
                        }

                        else_line =
                            j;
                    }
                }
            }


            /*
             * لم نجد نهاية_الشرط
             */
            if (condition_end < 0)
            {

                return -1;
            }


            /* ======================================================== */
            /* نهاية حلقة في مكان غير متوقع                               */
            /* ======================================================== */

            if (
                loop_is_end(
                    p
                )
            )
            {

                return -1;
            }
            if (
                condition_is_else(
                    p
                )
            )
            {
                return -1;
            }

            /*
             * تقييم الشرط
             */
            int result =
                condition_evaluate(
                    lines[condition_start]
                );


            if (result < 0)
                return -1;


            /*
             * ====================================================
             * الشرط صحيح
             * ====================================================
             */
            if (result == 1)
            {
                int body_start =
                    condition_start + 1;

                int body_end;


                /*
                 * إذا وجد والا،
                 * ننفذ ما قبل والا فقط.
                 */
                if (else_line >= 0)
                {
                    body_end =
                        else_line;
                }
                else
                {
                    body_end =
                        condition_end;
                }


                int body_count =
                    body_end -
                    body_start;


                if (body_count > 0)
                {
                    if (
                        arp_execute_lines(
                            &lines[body_start],
                            body_count
                        ) < 0
                    )
                    {
                        return -1;
                    }
                }
            }


            /*
             * ====================================================
             * الشرط خطأ
             * ====================================================
             */
            else
            {
                /*
                 * لا يوجد والا
                 */
                if (else_line < 0)
                {
                    /*
                     * لا نفذ شيئًا
                     */
                }
                else
                {
                    int body_start =
                        else_line + 1;

                    int body_count =
                        condition_end -
                        body_start;


                    if (body_count > 0)
                    {
                        if (
                            arp_execute_lines(
                                &lines[body_start],
                                body_count
                            ) < 0
                        )
                        {
                            return -1;
                        }
                    }
                }
            }


            /*
             * الانتقال بعد نهاية_الشرط
             */
            i =
                condition_end + 1;

            continue;
        }


        /* ======================================================== */
        /* نهاية شرط في مكان غير متوقع                               */
        /* ======================================================== */

        if (
            condition_is_end(
                p
            )
        )
        {

            return -1;
        }


        /* ======================================================== */
        /* تنفيذ الأمر العادي                                         */
        /* ======================================================== */

        if (
            arp_execute_line(
                line
            ) < 0
        )
        {
            return -1;
        }


        i++;
    }


    return 0;
}

/* ================================================================ */
/* المفسر الرئيسي                                                    */
/* ================================================================ */

int arp_execute_line(
    const char *line
)
{
    if (!line)
        return -1;


    const char *p =
        skip_spaces(line);


    /*
     * سطر فارغ
     */
    if (*p == '\0')
        return 0;


    /*
     * خلفية(0x00224488)
     */
    if (starts_with(p, "خلفية("))
    {
        return parse_background(p);
    }
    
    /*
     * متغير الاسم = القيمة
     */
    if (starts_with(p, "متغير"))
    {
        return parse_variable_declaration(p);
    }

    /*
     * احسب = ...
     */
    if (starts_with(p, "احسب"))
    {
        return parse_calculate(p);
    }

    /*
     * شغل(app.تطبيق)
     */
    if (
        starts_with(
            p,
            "شغل("
        )
    )
    {
        return parse_app(p);
    }

    /*
     * استدعاء مكتبة
     *
     * استدعاء(lib.تطبيق)
     */
    if (
        starts_with(
            p,
            "استدعاء("
        )
    )
    {
        return
            parse_library_import(
                p
            );
    }


    /*
     * مستطيل(x, y, width, height, color)
     */
    if (
        starts_with(
            p,
            "مستطيل("
        )
    )
    {
        return
            parse_rect(
                p
            );
    }


    /*
     * ================================================================
     * استدعاء دالة مع معاملات
     * ================================================================
     */

    const char *open =
        find_char(
            p,
            '('
        );


    if (open)
    {
        /*
         * استخراج اسم الدالة
         */
        char name[64];


        int name_length =
            open - p;


        /*
         * حذف المسافات من نهاية الاسم
         */
        while (
            name_length > 0 &&
            (
                p[name_length - 1] == ' ' ||
                p[name_length - 1] == '\t'
            )
        )
        {
            name_length--;
        }


        if (
            name_length <= 0 ||
            name_length >= 64
        )
        {
            return -1;
        }


        memcpy(
            name,
            p,
            name_length
        );


        name[name_length] =
            '\0';


        /*
         * البحث عن الدالة
         */
        arp_function_t *f =
            function_find(
                name
            );


        if (!f)
        {
            return -1;
        }


        /*
         * قراءة المعاملات
         */
        double arguments[
            ARP_MAX_PARAMETERS
        ];


        int argument_count = 0;


        if (
            parse_function_arguments(
                open + 1,
                arguments,
                &argument_count
            ) < 0
        )
        {

            return -1;
        }


        /*
         * التحقق من عدد المعاملات
         */
        if (
            argument_count !=
            f->parameter_count
        )
        {

            return -1;
        }


        /*
         * ربط القيم بأسماء المعاملات
         *
         * مثال:
         *
         * دالة(جمع, أ, ب)
         *
         * جمع(10, 20)
         *
         * أ = 10
         * ب = 20
         */
        for (
            int i = 0;
            i < f->parameter_count;
            i++
        )
        {
            if (
                variable_set(
                    f->parameters[i],
                    arguments[i]
                ) < 0
            )
            {

                return -1;
            }
        }


        /*
         * تنفيذ جسم الدالة
         */
        return
            arp_execute_lines(
                f->lines,
                f->line_count
            );
    }


    /*
     * أمر غير معروف
     */
    return -1;
}
