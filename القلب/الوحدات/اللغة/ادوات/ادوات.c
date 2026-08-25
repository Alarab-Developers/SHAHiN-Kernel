#include "ادوات.h"


/* ================================================================ */
/* قراءة سطر                                                          */
/* ================================================================ */

int read_line(
    const char *script,
    uint32_t size,
    uint32_t *offset,
    char *line
)
{
    if (
        !script ||
        !offset ||
        !line
    )
    {
        return -1;
    }


    int len = 0;


    while (
        *offset < size &&
        script[*offset] != '\n' &&
        script[*offset] != '\r'
    )
    {
        if (len >= 255)
            return -1;


        line[len++] =
            script[*offset];


        (*offset)++;
    }


    line[len] = '\0';


    /*
     * تخطي CR
     */
    if (
        *offset < size &&
        script[*offset] == '\r'
    )
    {
        (*offset)++;
    }


    /*
     * تخطي LF
     */
    if (
        *offset < size &&
        script[*offset] == '\n'
    )
    {
        (*offset)++;
    }


    return 0;
}


/* ================================================================ */
/* تخطي المسافات                                                     */
/* ================================================================ */

const char *skip_spaces(
    const char *p
)
{
    if (!p)
        return p;

    while (
        *p == ' '  ||
        *p == '\t' ||
        *p == '\r' ||
        *p == '\n'
    )
    {
        p++;
    }

    return p;
}


/* ================================================================ */
/* التحقق من بداية النص                                              */
/* ================================================================ */

int starts_with(
    const char *text,
    const char *prefix
)
{
    if (
        !text ||
        !prefix
    )
    {
        return 0;
    }


    while (*prefix)
    {
        if (*text != *prefix)
            return 0;

        text++;
        prefix++;
    }


    return 1;
}


/* ================================================================ */
/* تخطي بادئة                                                        */
/* ================================================================ */

const char *skip_prefix(
    const char *p,
    const char *prefix
)
{
    if (
        !p ||
        !prefix
    )
    {
        return p;
    }


    while (*prefix)
    {
        p++;
        prefix++;
    }


    return p;
}



const char *find_char(
    const char *text,
    char target
)
{
    if (!text)
        return 0;

    while (*text)
    {
        if (*text == target)
            return text;

        text++;
    }

    return 0;
}



/* ================================================================ */
/* تحويل double إلى نص                                               */
/* ================================================================ */

void double_to_string(
    double value,
    char *buffer,
    int precision
)
{
    if (!buffer)
        return;

    int pos = 0;


    /*
     * الرقم السالب
     */
    if (value < 0)
    {
        buffer[pos++] = '-';

        value = -value;
    }


    /*
     * استخراج الجزء الصحيح
     */
    uint64_t integer =
        (uint64_t)value;


    char temp[32];

    int count = 0;


    /*
     * الحالة الخاصة للصفر
     */
    if (integer == 0)
    {
        temp[count++] = '0';
    }
    else
    {
        while (integer > 0)
        {
            temp[count++] =
                '0' +
                (integer % 10);

            integer /= 10;
        }
    }


    /*
     * عكس الأرقام
     */
    while (count > 0)
    {
        buffer[pos++] =
            temp[--count];
    }


    /*
     * الجزء الكسري
     */
    double fraction =
        value -
        (uint64_t)value;


    if (
        fraction > 0 &&
        precision > 0
    )
    {
        buffer[pos++] = '.';


        for (
            int i = 0;
            i < precision;
            i++
        )
        {
            fraction *= 10;

            int digit =
                (int)fraction;


            buffer[pos++] =
                '0' +
                digit;


            fraction -= digit;


            /*
             * إذا انتهى الجزء الكسري
             */
            if (fraction == 0)
                break;
        }
    }


    buffer[pos] = '\0';
}


/* ================================================================ */
/* تحويل نص إلى رقم عشري                                             */
/* ================================================================ */

double string_to_double(
    const char *text,
    const char **end
)
{
    if (!text)
    {
        if (end)
            *end = text;

        return 0;
    }


    const char *p =
        text;


    /*
     * تخطي المسافات
     */
    p =
        skip_spaces(
            p
        );


    int sign = 1;


    /*
     * الإشارة
     */
    if (*p == '-')
    {
        sign = -1;

        p++;
    }
    else if (*p == '+')
    {
        p++;
    }


    double result = 0;

    int digits = 0;


    /*
     * الجزء الصحيح
     */
    while (
        *p >= '0' &&
        *p <= '9'
    )
    {
        result =
            result * 10 +
            (*p - '0');

        p++;

        digits++;
    }


    /*
     * الجزء الكسري
     */
    if (*p == '.')
    {
        p++;

        double divisor =
            10.0;


        while (
            *p >= '0' &&
            *p <= '9'
        )
        {
            result +=
                (*p - '0') /
                divisor;

            divisor *= 10.0;

            p++;

            digits++;
        }
    }


    /*
     * لم يتم العثور على رقم
     */
    if (digits == 0)
    {
        if (end)
            *end = text;

        return 0;
    }


    if (end)
        *end = p;


    return
        result * sign;
}
