#include "الشروط.h"
#include "رياضيات.h"
#include "المتغيرات.h"
#include "../../../ادوات/ادوات.h"


/* ================================================================ */
/* التحقق من بداية الشرط                                             */
/* ================================================================ */

int condition_is_start(
    const char *line
)
{
    if (!line)
        return 0;

    const char *p =
        skip_spaces(line);

    return starts_with(
        p,
        "اذا("
    );
}


/* ================================================================ */
/* التحقق من والا                                                     */
/* ================================================================ */

int condition_is_else(
    const char *line
)
{
    if (!line)
        return 0;

    const char *p =
        skip_spaces(line);

    return strcmp(
        p,
        "والا"
    ) == 0;
}



/* ================================================================ */
/* التحقق من نهاية الشرط                                             */
/* ================================================================ */

int condition_is_end(
    const char *line
)
{
    if (!line)
        return 0;

    const char *p =
        skip_spaces(line);

    return strcmp(
        p,
        "نهاية_الشرط"
    ) == 0;
}


/* ================================================================ */
/* تقييم المقارنة                                                    */
/* ================================================================ */

int condition_evaluate(
    const char *condition
)
{
    if (!condition)
        return -1;


    const char *p =
        skip_spaces(condition);


    /*
     * يجب أن يبدأ بـ:
     *
     * اذا(
     */
    if (!starts_with(p, "اذا("))
    {

        return -1;
    }


    p =
        skip_prefix(
            p,
            "اذا("
        );


    p =
        skip_spaces(p);


    /*
     * البحث عن )
     */
    const char *close =
        find_char(p, ')');


    if (!close)
    {
        return -1;
    }


    /*
     * يجب ألا يوجد شيء بعد )
     */
    const char *after =
        skip_spaces(close + 1);


    if (*after != '\0')
    {
        return -1;
    }


    /*
     * نسخ التعبير داخل:
     *
     * اذا(...)
     */
    char expression[256];


    int length =
        close - p;


    if (
        length <= 0 ||
        length >= 256
    )
    {
        return -1;
    }


    memcpy(
        expression,
        p,
        length
    );


    expression[length] =
        '\0';


    /*
     * البحث عن =
     *
     * الصيغة الحالية:
     *
     * س = 2
     */
    const char *equal =
        find_char(
            expression,
            '='
        );


    if (!equal)
    {
        return -1;
    }


    /*
     * منع:
     *
     * س == 2
     *
     * في النسخة الحالية.
     */
    if (equal[1] == '=')
    {
        return -1;
    }


    /*
     * الطرف الأيسر
     *
     * س
     */
    char left[128];


    int left_length =
        equal - expression;


    if (
        left_length <= 0 ||
        left_length >= 128
    )
    {
        return -1;
    }


    memcpy(
        left,
        expression,
        left_length
    );


    left[left_length] =
        '\0';


    /*
     * الطرف الأيمن
     *
     * 2
     */
    const char *right =
        skip_spaces(
            equal + 1
        );


    if (*right == '\0')
    {
        return -1;
    }


    /*
     * حساب الطرفين
     */
    int error_left = 0;
    int error_right = 0;


    double left_value =
        math_calculate(
            left,
            &error_left
        );


    double right_value =
        math_calculate(
            right,
            &error_right
        );


    if (
        error_left ||
        error_right
    )
    {
        return -1;
    }


    /*
     * المقارنة
     *
     * مثال:
     *
     * س = 2
     *
     * إذا كانت:
     *
     * 2 = 2
     *
     * النتيجة 1
     */
    if (left_value == right_value)
        return 1;


    return 0;
}

