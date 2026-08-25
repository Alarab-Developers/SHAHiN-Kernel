#include "اوامر_اللغة.h"
#include "../المحلل.h"
#include "../../../لغة.h"
#include "../../../ادوات/ادوات.h"
#include "../../../المنفذ/المنفذ.h"
#include "../مفاهيم_اللغة/رياضيات.h"
#include "../مفاهيم_اللغة/المتغيرات.h"
#include "../مفاهيم_اللغة/مكتبات_اللغة.h"




/* ================================================================ */
/* تحليل أمر متغير                                                   */
/* ================================================================ */

int parse_variable_declaration(
    const char *p
)
{
    /*
     * تخطي كلمة متغير
     */
    p =
        skip_prefix(
            p,
            "متغير"
        );


    p =
        skip_spaces(
            p
        );


    /*
     * قراءة اسم المتغير
     */
    char name[64];

    int length = 0;


    while (
        *p &&
        *p != ' ' &&
        *p != '\t' &&
        *p != '=' &&
        length < 63
    )
    {
        name[length++] =
            *p++;
    }


    name[length] = '\0';


    if (length == 0)
    {
        return -1;
    }


    p =
        skip_spaces(
            p
        );


    /*
     * يجب وجود =
     */
    if (*p != '=')
    {
        return -1;
    }


    p++;


    p =
        skip_spaces(
            p
        );


    if (*p == '\0')
    {
        return -1;
    }


    /*
     * حساب القيمة
     *
     * يسمح بهذا:
     *
     * متغير ق = 5
     *
     * وكذلك:
     *
     * متغير س = 5 + 10
     */
    int error = 0;


    double value =
        math_calculate(
            p,
            &error
        );


    if (error)
    {
        return -1;
    }


    /*
     * حفظ المتغير
     */
    if (
        variable_set(
            name,
            value
        ) < 0
    )
    {
        return -1;
    }


    return 0;
}

/* ================================================================ */
/* تحليل استدعاء مكتبة                                               */
/* ================================================================ */

int parse_library_import(
    const char *p
)
{
    /*
     * تخطي:
     *
     * استدعاء(
     */
    p =
        skip_prefix(
            p,
            "استدعاء("
        );


    p =
        skip_spaces(
            p
        );


    char name[128];

    int length = 0;


    /*
     * قراءة اسم المكتبة
     *
     * مثال:
     *
     * lib.تطبيق
     */
    while (
        *p &&
        *p != ')' &&
        *p != ' ' &&
        *p != '\t' &&
        length < 127
    )
    {
        name[length++] =
            *p++;
    }


    name[length] =
        '\0';


    p =
        skip_spaces(
            p
        );


    /*
     * يجب وجود )
     */
    if (*p != ')')
    {

        return -1;
    }


    /*
     * التحقق من وجود اسم
     */
    if (length == 0)
    {

        return -1;
    }


    /*
     * لا يجب وجود شيء بعد )
     */
    p++;

    p =
        skip_spaces(
            p
        );


    if (*p != '\0')
    {
        return -1;
    }


    return
        library_import(
            name
        );
}

/* ================================================================ */
/* تحليل أمر احسب                                                    */
/* ================================================================ */

int parse_calculate(
    const char *p
)
{
    /*
     * تخطي كلمة:
     *
     * احسب
     */
    p =
        skip_prefix(
            p,
            "احسب"
        );


    p =
        skip_spaces(
            p
        );


    /*
     * يجب وجود =
     */
    if (*p != '=')
    {
        return -1;
    }


    p++;


    p =
        skip_spaces(
            p
        );


    /*
     * لا توجد معادلة
     */
    if (*p == '\0')
    {
        return -1;
    }


    int error = 0;


    double result =
        math_calculate(
            p,
            &error
        );


    if (error)
    {
        return -1;
    }


    return
        execute_calculate(
            result
        );
}

/* ================================================================ */
/* تحليل أمر شغل                                                     */
/* ================================================================ */

int parse_app(
    const char *p
)
{
    p =
        skip_prefix(
            p,
            "شغل("
        );

    p =
        skip_spaces(
            p
        );


    char name[256];

    int length = 0;


    /*
     * قراءة المسار الكامل
     *
     * أمثلة:
     *
     * البرنامج.تطبيق
     *
     * التطبيقات/البرنامج.تطبيق
     */
    while (
        *p &&
        *p != ')' &&
        length < 255
    )
    {
        name[length++] =
            *p++;
    }


    name[length] =
        '\0';


    /*
     * حذف المسافات في نهاية المسار
     */
    while (
        length > 0 &&
        (
            name[length - 1] == ' ' ||
            name[length - 1] == '\t'
        )
    )
    {
        name[--length] =
            '\0';
    }


    if (length == 0)
    {

        return -1;
    }


    if (*p != ')')
    {

        return -1;
    }


    p++;


    p =
        skip_spaces(
            p
        );


    if (*p != '\0')
    {

        return -1;
    }


    return
        execute_app(
            name
        );
}


/* ================================================================ */
/* تحليل أمر مستطيل                                                  */
/* ================================================================ */

int parse_rect(
    const char *p
)
{
    if (!p)
        return -1;


    /*
     * تخطي:
     *
     * مستطيل(
     */
    p =
        skip_prefix(
            p,
            "مستطيل("
        );


    double x;
    double y;
    double width;
    double height;
    double color;


    /*
     * س
     */
    if (
        parse_numeric_argument(
            &p,
            &x,
            0
        ) < 0
    )
    {

        return -1;
    }


    /*
     * ص
     */
    if (
        parse_numeric_argument(
            &p,
            &y,
            0
        ) < 0
    )
    {

        return -1;
    }


    /*
     * العرض
     */
    if (
        parse_numeric_argument(
            &p,
            &width,
            0
        ) < 0
    )
    {
        return -1;
    }


    /*
     * الارتفاع
     */
    if (
        parse_numeric_argument(
            &p,
            &height,
            0
        ) < 0
    )
    {

        return -1;
    }


    /*
     * اللون
     */
    if (
        parse_numeric_argument(
            &p,
            &color,
            1
        ) < 0
    )
    {

        return -1;
    }


    /*
     * يجب أن تنتهي بـ )
     */
    if (*p != ')')
    {

        return -1;
    }


    p++;


    p =
        skip_spaces(
            p
        );


    /*
     * لا شيء بعد الأمر
     */
    if (*p != '\0')
    {

        return -1;
    }


    /*
     * تنفيذ المستطيل
     */
    return
        execute_rect(
            (int)x,
            (int)y,
            (int)width,
            (int)height,
            (uint32_t)color
        );
}

/* ================================================================ */
/* تحليل أمر خلفية                                                   */
/* ================================================================ */
int parse_background(
    const char *p
)
{
    /*
     * تخطي:
     *
     * خلفية(
     */
    p = skip_prefix(p, "خلفية(");

    p = skip_spaces(p);


    uint32_t color = 0;


    /*
     * يجب أن يبدأ اللون بـ 0x
     */
    if (
        p[0] != '0' ||
        (p[1] != 'x' &&
         p[1] != 'X')
    )
    {

        return -1;
    }

    p += 2;


    int digits = 0;


    /*
     * قراءة الرقم الست عشري
     */
    while (*p)
    {
        char c = *p;

        uint32_t value;


        if (c >= '0' && c <= '9')
        {
            value = c - '0';
        }
        else if (c >= 'A' && c <= 'F')
        {
            value = c - 'A' + 10;
        }
        else if (c >= 'a' && c <= 'f')
        {
            value = c - 'a' + 10;
        }
        else
        {
            break;
        }


        color =
            (color << 4) |
            value;

        digits++;

        p++;
    }


    /*
     * لم نجد أرقامًا
     */
    if (digits == 0)
    {

        return -1;
    }


    p = skip_spaces(p);


    /*
     * يجب أن تنتهي بـ )
     */
    if (*p != ')')
    {

        return -1;
    }


    /*
     * إرسال الأمر إلى المنفذ
     */
    return execute_background(color);
}
