#include "بيانات_التطبيق.h"

#include <string.h>

void* app_data_ptr(
    file_t* f
)
{
    return
        (uint8_t*)f->data +
        sizeof(aros_header_t);
}

uint64_t app_data_size(
    file_t* f
)
{
    aros_header_t* hdr =
        (aros_header_t*)f->data;

    return hdr->data_size;
}

void app_data_copy(
    file_t* f,
    void* dst
)
{
    memcpy(
        dst,
        app_data_ptr(f),
        app_data_size(f)
    );
}
