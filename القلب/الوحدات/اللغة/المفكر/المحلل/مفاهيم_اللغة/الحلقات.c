#include "الحلقات.h"
#include "الدوال.h"
#include "../../../ادوات/ادوات.h"
#include "../../المحلل/المحلل.h"



/* ================================================================ */
/* هل هذا بداية حلقة؟                                                */
/* ================================================================ */

int loop_is_start(
    const char *line
)
{
    if (!line)
        return 0;


    const char *p =
        skip_spaces(
            line
        );


    /*
     * الشكل المطلوب:
     *
     * حلقة()
     */
    if (
        !starts_with(
            p,
            "حلقة("
        )
    )
    {
        return 0;
    }


    p =
        skip_prefix(
            p,
            "حلقة("
        );


    p =
        skip_spaces(
            p
        );


    /*
     * يجب أن تكون:
     *
     * حلقة()
     */
    if (*p != ')')
    {
        return 0;
    }


    p++;


    p =
        skip_spaces(
            p
        );


    /*
     * لا يسمح بشيء بعد )
     */
    if (*p != '\0')
    {
        return 0;
    }


    return 1;
}


/* ================================================================ */
/* هل هذا نهاية حلقة؟                                                */
/* ================================================================ */

int loop_is_end(
    const char *line
)
{
    if (!line)
        return 0;


    const char *p =
        skip_spaces(
            line
        );


    if (
        !starts_with(
            p,
            "نهاية_الحلقة"
        )
    )
    {
        return 0;
    }


    p =
        skip_prefix(
            p,
            "نهاية_الحلقة"
        );


    p =
        skip_spaces(
            p
        );


    /*
     * يجب ألا يوجد شيء بعد نهاية_الحلقة
     */
    if (*p != '\0')
    {
        return 0;
    }


    return 1;
}


/* ================================================================ */
/* تنفيذ جسم الحلقة                                                  */
/* ================================================================ */

int loop_execute(
    const char lines[][ARP_MAX_LINE_LENGTH],
    int line_count
)
{
    if (
        !lines ||
        line_count < 0
    )
    {
        return -1;
    }


    /*
     * حلقة لا نهائية
     */
    while (1)
    {
        if (
            arp_execute_lines(
                lines,
                line_count
            ) < 0
        )
        {
            return -1;
        }
    }


    return 0;
}
