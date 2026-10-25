#ifndef الشاشة_الحرجة_H
#define الشاشة_الحرجة_H

#include "القلب/المكتبات/المكتبات.h"

void kernel_panic(const char* msg);

void panic_page_fault(
    uint64_t addr,
    uint64_t* stack
);

void generic_exception_handler_c(uint64_t* stack);

/* =========================================================
 * النص — خط 8x8 على البافر الخلفي
 * (التعريف في الشاشة_الحرجة.c، ويعتمد على getters المحرك)
 * ========================================================= */

void draw_char(int x, int y, char c, uint32_t color);
void draw_string(int x, int y, const char *text, uint32_t color);

#endif
