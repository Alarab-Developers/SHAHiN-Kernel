#ifndef FRAME_ALLOCATOR_H
#define FRAME_ALLOCATOR_H

#include "القلب/المكتبات/المكتبات.h"

void frame_allocator_init(void);
void frame_allocator_set_ram_size(uint64_t total_ram_bytes);
uint64_t alloc_frame(void);
void free_frame(uint64_t frame);

/* حجم الرام الفعلي (كما وصل من الـ bootloader)، يُستخدم لتحديد
   حجم الـ identity map في paging_init. */
extern uint64_t g_total_ram_bytes;

#endif
