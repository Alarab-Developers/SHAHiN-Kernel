#ifndef FRAME_ALLOCATOR_H
#define FRAME_ALLOCATOR_H

#include "القلب/المكتبات/المكتبات.h"

void frame_allocator_init(void);

uint64_t alloc_frame(void);

void free_frame(uint64_t frame);

void reserve_frame_range(uint64_t phys_start, uint64_t phys_end);

#endif
