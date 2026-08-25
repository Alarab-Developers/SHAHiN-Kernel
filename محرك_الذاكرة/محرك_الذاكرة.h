#ifndef MEMORY_H
#define MEMORY_H

#include "القلب/المكتبات/المكتبات.h"

#define PAGE_SIZE         4096
#define HEAP_START 0x8000000ULL
#define HIGHER_HALF_BASE 0

#include "الأدارة/مدير_الكومة.h"

void memory_init();

#endif
