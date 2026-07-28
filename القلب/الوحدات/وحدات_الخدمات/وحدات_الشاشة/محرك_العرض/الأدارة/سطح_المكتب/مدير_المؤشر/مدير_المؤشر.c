#include "مدير_المؤشر.h"

static cursor_t cursor;

void cursor_init(void)
{
    cursor.width  = 10;
    cursor.height = 10;
    cursor.color  = 0x00FFFFFF;
}

const cursor_t* cursor_get(void)
{
    return &cursor;
}
