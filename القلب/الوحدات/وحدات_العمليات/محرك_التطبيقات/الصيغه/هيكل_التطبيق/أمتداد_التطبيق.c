#include "أمتداد_التطبيق.h"

#include <string.h>

int app_format_valid(
    file_t* f
)
{
    if (!f)
        return 0;

    if (!f->data)
        return 0;

    aros_header_t* hdr =
        (aros_header_t*)f->data;

    return strcmp(
        hdr->format,
        APP_FORMAT
    ) == 0;
}
