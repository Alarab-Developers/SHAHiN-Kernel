#include "محرك_التطبيقات.h"
#include "القلب/الوحدات/وحدات_العمليات/محرك_التطبيقات/الصيغه/تطبيق.h"

void app_run(file_t* f)
{
    if (!f)
        return;

    aros_run(f);
}
