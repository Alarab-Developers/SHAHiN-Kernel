#ifndef الشاشة_الحرجة_H
#define الشاشة_الحرجة_H

#include "القلب/المكتبات/المكتبات.h"

void kernel_panic(const char* msg);

void panic_page_fault(
    uint64_t addr,
    uint64_t* stack
);

void generic_exception_handler_c(uint64_t* stack);

#endif
