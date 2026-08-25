#include "المنفذ.h"

#include "../لغة.h"
#include "../ادوات/ادوات.h"
#include "القلب/الوحدات/محرك_الفيديو/بوابة_الفيديو.h"
#include "القلب/الوحدات/محرك_الفيديو/محرك_الفيديو.h"
#include "القلب/الوحدات/محمل_التطبيقات/محمل_التطبيقات.h"
#include "../المفكر/المحلل/مفاهيم_اللغة/رياضيات.h"



/* ================================================================ */
/* تنفيذ background                                                  */
/* ================================================================ */

int execute_background(
    uint32_t color
)
{
    video_api.clear(color);

    video_api.swap();

    return 0;
}






int execute_calculate(
    double result
)
{
    char buffer[64];


    /*
     * تحويل النتيجة إلى نص
     */
    double_to_string(
        result,
        buffer,
        6
    );


    return 0;
}



/* ================================================================ */
/* تشغيل تطبيق                                                        */
/* ================================================================ */

int execute_app(
    const char *name
)
{
    if (!name || !name[0])
        return -1;

    return loader_app(name);
}
// الرسم
int execute_rect(
    int x,
    int y,
    int width,
    int height,
    uint32_t color
)
{
    if (
        width <= 0 ||
        height <= 0
    )
    {
        return -1;
    }


    fill_rect(
        x,
        y,
        width,
        height,
        color
    );


    return 0;
}
