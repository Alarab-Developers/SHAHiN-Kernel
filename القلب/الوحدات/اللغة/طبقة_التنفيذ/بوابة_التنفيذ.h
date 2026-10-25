#ifndef المنفذ_H
#define المنفذ_H

#include "القلب/المكتبات/انواع.h"

/* ============================================================ */
/* التطبيقات                                                    */
/* ============================================================ */

int execute_app(const char *name);

/* ============================================================ */
/* حلقة الأحداث والعرض                                          */
/* ============================================================ */

int  execute_present_screen(void);

/* ============================================================ */
/* العتاد                                                       */
/* ============================================================ */

int     execute_hardware(uint16_t port, uint8_t value);
uint8_t execute_read_hardware(uint16_t port);

/* ============================================================ */
/* الذاكرة الخام                                                */
/* ============================================================ */

int execute_memory_write   (uint64_t address, uint32_t value);
int execute_memory_read    (uint64_t address, uint32_t *value);
int execute_memory_write8  (uint64_t address, uint8_t  value);
int execute_memory_read8   (uint64_t address, uint8_t  *value);
int execute_memory_write16 (uint64_t address, uint16_t value);
int execute_memory_read16  (uint64_t address, uint16_t *value);
int execute_memory_write64 (uint64_t address, uint64_t value);
int execute_memory_read64  (uint64_t address, uint64_t *value);

int execute_memory_copy    (uint64_t dest, uint64_t src, uint64_t size);
int execute_memory_set     (uint64_t address, uint8_t value, uint64_t size);
int execute_memory_set32   (uint64_t address, uint32_t value, uint64_t count);
int execute_memory_compare (uint64_t addr1, uint64_t addr2,
                            uint64_t size, int *result);

int execute_memory_alloc   (uint64_t size, uint64_t *address);
int execute_memory_free    (uint64_t address);
int execute_memory_alloc_page(uint64_t *address);
int execute_memory_map     (uint64_t virt, uint64_t phys, uint64_t flags);

/* ============================================================ */
/* الوقت والإدخال                                               */
/* ============================================================ */

int  execute_wait(uint64_t count);
int  execute_keyboard_value(unsigned char key);
int  execute_input(const char *var_name);
int  execute_rtc_get(int field, int *value);

/* ============================================================ */
/* النصوص                                                       */
/* ============================================================ */

int  execute_text_len (const char *text);
int  execute_text_char(const char *text, int index);

/* ============================================================ */
/* الملفات                                                      */
/* ============================================================ */

uint64_t execute_file_pointer(const char *name);

/* ============================================================ */
/* الطباعة                                                      */
/* ============================================================ */

int execute_print_line       (const char *text);
int execute_print_line_color (const char *text, uint32_t color);
int execute_print_inline     (const char *text);
int execute_print_inline_color(const char *text, uint32_t color);

/* ============================================================ */
/* الفيديو والبافر                                              */
/* ============================================================ */

uint64_t execute_get_back_buffer_ptr (void);
uint64_t execute_get_front_buffer_ptr(void);

uint8_t  execute_byte_at(uint64_t address);

#endif
