#include "لغة.h"
#include "ادوات/ادوات.h"
#include "المفكر/المحلل/المحلل.h"
#include "المفكر/المحلل/مفاهيم_اللغة/الدوال.h"
#include "المفكر/المحلل/مفاهيم_اللغة/المتغيرات.h"



static char body_storage[
    ARP_MAX_FUNCTION_LINES
][ARP_MAX_LINE_LENGTH];

static const char *body[
    ARP_MAX_FUNCTION_LINES
];


/* ================================================================ */
/* تنفيذ سطر واحد                                                    */
/* ================================================================ */

static int run_line(
    const char *line
)
{
    return arp_execute_line(line);
}

/* ================================================================ */
/* استخراج اسم الدالة                                                 */
/* ================================================================ */

static int parse_function_definition(
    const char *line,
    char *name,
    char parameters[][64],
    int *parameter_count
)
{
    if (
        !line ||
        !name ||
        !parameters ||
        !parameter_count
    )
    {
        return -1;
    }


    *parameter_count = 0;


    const char *p =
        skip_spaces(line);


    /*
     * يجب أن يبدأ بـ:
     *
     * دالة(
     */
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
        name_length--;
    }


    name[name_length] =
        '\0';


    if (name_length == 0)
        return -1;


    /*
     * لا توجد معاملات
     *
     * دالة(اختبار)
     */
    if (*p == ')')
    {
        p++;


        p =
            skip_spaces(p);


        if (*p != '\0')
            return -1;


        return 0;
    }


    /*
     * يجب أن يوجد ,
     */
    if (*p != ',')
        return -1;


    /*
     * قراءة المعاملات
     */
    while (*p)
    {
        /*
         * تخطي الفاصلة
         */
        if (*p == ',')
            p++;


        p =
            skip_spaces(p);


        /*
         * نهاية التعريف
         */
        if (*p == ')')
        {
            p++;

            p =
                skip_spaces(p);


            if (*p != '\0')
                return -1;


            return 0;
        }


        if (
            *parameter_count >=
            ARP_MAX_PARAMETERS
        )
        {
            return -1;
        }


        int length = 0;


        /*
         * قراءة اسم المعامل
         */
        while (
            *p &&
            *p != ',' &&
            *p != ')' &&
            length < 63
        )
        {
            parameters[
                *parameter_count
            ][length++] =
                *p++;
        }


        /*
         * حذف المسافات من نهاية الاسم
         */
        while (
            length > 0 &&
            (
                parameters[
                    *parameter_count
                ][length - 1] == ' ' ||

                parameters[
                    *parameter_count
                ][length - 1] == '\t'
            )
        )
        {
            length--;
        }


        parameters[
            *parameter_count
        ][length] =
            '\0';


        if (length == 0)
            return -1;


        (*parameter_count)++;


        /*
         * إذا وصلنا إلى )
         */
        if (*p == ')')
        {
            p++;

            p =
                skip_spaces(p);


            if (*p != '\0')
                return -1;


            return 0;
        }


        /*
         * يجب أن يكون التالي ,
         */
        if (*p != ',')
            return -1;
    }


    return -1;
}


/* ================================================================ */
/* تشغيل السكربت                                                     */
/* ================================================================ */

static int execute_script(
    const char *script,
    uint32_t size
)
{
    if (
        !script ||
        size == 0
    )
    {
        return -1;
    }


    uint32_t offset = 0;


    while (offset < size)
    {
        char line[ARP_MAX_LINE_LENGTH];


        /*
         * قراءة سطر
         */
        if (
            read_line(
                script,
                size,
                &offset,
                line
            ) < 0
        )
        {
            return -1;
        }


        const char *p =
            skip_spaces(line);


        /*
         * تجاهل السطر الفارغ
         */
        if (*p == '\0')
        {
            continue;
        }


        /* ======================================================== */
        /* تعريف دالة                                                */
        /* ======================================================== */

        if (starts_with(p, "دالة("))
        {
            char name[64];

            char parameters[
                ARP_MAX_PARAMETERS
            ][
                64
            ];

            int parameter_count = 0;


            if (
                parse_function_definition(
                    p,
                    name,
                    parameters,
                    &parameter_count
                ) < 0
            )
            {
                return -1;
            }


            /*
             * تخزين جسم الدالة
             *
             * body_storage و body أصبحتا static على مستوى الملف
             * (راجع التعليق في أعلى الملف) بدل أن تكونا متغيرين
             * محليين هنا، لتفادي حجز 256+ كيلوبايت على المكدس.
             */
            int body_count = 0;


            int found_end = 0;


            /* ==================================================== */
            /* قراءة جسم الدالة                                      */
            /* ==================================================== */

            while (offset < size)
            {
                char body_line[
                    ARP_MAX_LINE_LENGTH
                ];


                /*
                 * قراءة السطر التالي
                 */
                if (
                    read_line(
                        script,
                        size,
                        &offset,
                        body_line
                    ) < 0
                )
                {
                    return -1;
                }


                const char *b =
                    skip_spaces(
                        body_line
                    );


                /*
                 * تجاهل الأسطر الفارغة
                 */
                if (*b == '\0')
                {
                    continue;
                }


                /* ================================================= */
                /* نهاية الدالة                                      */
                /* ================================================= */

                if (
                    starts_with(
                        b,
                        "نهاية("
                    )
                )
                {
                    char end_name[64];


                    const char *e =
                        skip_prefix(
                            b,
                            "نهاية("
                        );


                    e =
                        skip_spaces(
                            e
                        );


                    int end_len = 0;


                    /*
                     * قراءة اسم الدالة الموجودة
                     * داخل نهاية(...)
                     */
                    while (
                        *e &&
                        *e != ')' &&
                        end_len < 63
                    )
                    {
                        end_name[end_len++] =
                            *e++;
                    }


                    end_name[end_len] =
                        '\0';


                    /*
                     * يجب وجود )
                     */
                    if (*e != ')')
                    {
                        return -1;
                    }


                    e++;


                    /*
                     * تخطي المسافات
                     */
                    e =
                        skip_spaces(
                            e
                        );


                    /*
                     * لا يسمح بأي شيء بعد نهاية(...)
                     */
                    if (*e != '\0')
                    {


                        return -1;
                    }


                    /*
                     * التحقق من اسم الدالة
                     *
                     * دالة(اختبار)
                     *
                     * نهاية(اختبار)
                     */
                    if (
                        strcmp(
                            name,
                            end_name
                        ) != 0
                    )
                    {
                        return -1;
                    }


                    found_end = 1;

                    break;
                }


                /* ================================================= */
                /* التحقق من حجم جسم الدالة                         */
                /* ================================================= */

                if (
                    body_count >=
                    ARP_MAX_FUNCTION_LINES
                )
                {

                    return -1;
                }


                /*
                 * حفظ السطر كما هو
                 *
                 * لا نحذف المسافات هنا.
                 * المفسر الداخلي هو المسؤول عن تحليل السطر.
                 */
                strcpy(
                    body_storage[body_count],
                    body_line
                );


                body[body_count] =
                    body_storage[body_count];


                body_count++;
            }


            /* ===================================================== */
            /* لم نجد نهاية الدالة                                   */
            /* ===================================================== */

            if (!found_end)
            {

                return -1;
            }


            /* ===================================================== */
            /* تسجيل الدالة                                           */
            /* ===================================================== */

            if (
                function_register(
                    name,
                    parameters,
                    parameter_count,
                    body,
                    body_count
                ) < 0
            )
            {

                return -1;
            }


            /*
             * لا ننفذ جسم الدالة الآن.
             *
             * فقط نسجلها.
             */
            continue;
        }


        /* ======================================================== */
        /* سطر عادي                                                  */
        /* ======================================================== */

        if (
            arp_execute_line(
                line
            ) < 0
        )
        {
            return -1;
        }
    }


    return 0;
}


/* ================================================================ */
/* تشغيل البرنامج الرئيسي                                            */
/* ================================================================ */

static int run_script(
    const char *script,
    uint32_t size
)
{
    functions_init();
    variables_init();


    return execute_script(
        script,
        size
    );
}

/* ================================================================ */
/* تشغيل مكتبة                                                        */
/* ================================================================ */

int arp_run_library(
    const char *script,
    uint32_t size
)
{
    /*
     * مهم:
     *
     * لا نستدعي:
     *
     * functions_init();
     *
     * لأن المكتبة يجب أن تضيف دوالها
     * إلى الدوال الموجودة
     */


    return execute_script(
        script,
        size
    );
}

/* ================================================================ */
/* بوابة اللغة                                                       */
/* ================================================================ */

arp_api_t arp_api =
{
    .run_line   = run_line,
    .run_script = run_script
};
