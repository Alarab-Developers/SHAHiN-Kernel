#include "محرك_الذاكرة.h"
#include "الأدارة/مدير_الكومة.h"
#include "الأدارة/مدير_الاطار.h"

void memory_init() {

    frame_allocator_init();

    heap_init();
}
