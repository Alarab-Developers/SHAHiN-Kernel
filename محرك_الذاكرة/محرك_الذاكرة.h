#ifndef MEMORY_H
#define MEMORY_H

#include "القلب/المكتبات/المكتبات.h"
#include "الأدارة/مدير_الكومة.h"

#define PAGE_SIZE         4096    //حجم الصفحة الواحدة 4 غيغا بايت 
#define HEAP_START 0x8000000ULL  // عنوان بدء عمل الصفحات الافترضي
#define HIGHER_HALF_BASE 0      // القاعدة أو نقطة الانطلاق للنصف العلوي من الذاكرة

void memory_init();            // تصريح دالة التهية

#endif
