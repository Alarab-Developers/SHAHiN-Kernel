#ifndef المنفذ_H
#define المنفذ_H

#include "القلب/المكتبات/انواع.h"

/*
 * تنفيذ أوامر للغة
 */
int execute_background(uint32_t color);
int execute_calculate(double result);
int execute_app(const char *name);
int execute_rect(int x, int y, int width, int height, uint32_t color);
int execute_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);
int execute_present(void);

int execute_hardware( uint16_t port, uint8_t value);
uint8_t execute_read_hardware(uint16_t port);

int execute_memory_write(uint64_t address,uint32_t value);
int execute_memory_read(uint64_t address, uint32_t *value);

/*
 * التحكم الكامل بالذاكرة الخام: أحجام مختلفة، نسخ/تعبئة/مقارنة،
 * تخصيص/تحرير من الكومة، تخصيص صفحة فيزيائية، وربط virt<->phys
 */
int execute_memory_write8(uint64_t address, uint8_t value);
int execute_memory_read8(uint64_t address, uint8_t *value);
int execute_memory_write16(uint64_t address, uint16_t value);
int execute_memory_read16(uint64_t address, uint16_t *value);
int execute_memory_write64(uint64_t address, uint64_t value);
int execute_memory_read64(uint64_t address, uint64_t *value);

int execute_memory_copy(uint64_t dest, uint64_t src, uint64_t size);
int execute_memory_set(uint64_t address, uint8_t value, uint64_t size);
int execute_memory_compare(uint64_t addr1, uint64_t addr2, uint64_t size, int *result);

int execute_memory_alloc(uint64_t size, uint64_t *address);
int execute_memory_free(uint64_t address);
int execute_memory_alloc_page(uint64_t *address);
int execute_memory_map(uint64_t virt, uint64_t phys, uint64_t flags);

/*
 * الانتظار
 *
 * count = عدد نبضات المؤقت
 */
int execute_wait(uint64_t count);
int execute_keyboard_value(unsigned char key);
int execute_print(const char *text);
int execute_input(const char *var_name);

#endif
