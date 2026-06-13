#include "محرك_التطبيقات.h"
#include "القلب/محركات_الخدمات/محرك_التطبيقات/الصيغه/تطبيق.h"

void app_run(file_t* f)
{
    if (!f)
        return;

    aros_run(f);
}
