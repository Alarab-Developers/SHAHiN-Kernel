#ifndef مترجم_AOT_H
#define مترجم_AOT_H

#include "شجرة.h"
#include "القلب/المكتبات/المكتبات.h"

/* =========================================================
 * AOT — ترجمة مسبقة إلى ملف تنفيذي
 * ========================================================= */

#define ARP_EXEC_MAGIC   0x45505241u  /* "ARPE" little-endian */

#define ARP_EXEC_VERSION 20

/* رأس الملف التنفيذي */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_offset;
    uint32_t entry_size;
    uint32_t strings_offset;
    uint32_t strings_size;
    uint32_t function_count;
    uint32_t flags;
    uint32_t code_offset;
    uint32_t code_size;
    uint32_t reloc_offset;
    uint32_t reloc_count;
} arp_exec_header_t;

/* رأس دالة داخل الملف */
typedef struct {
    uint32_t name_offset;
    uint32_t name_len;
    uint32_t code_offset;
    uint32_t code_size;
    uint16_t param_count;
    uint16_t flags;
    uint32_t reloc_first;
    uint32_t reloc_count;
    uint32_t params_offset;   /* أسماء المعاملات في قسم النصوص مفصولة بـ '\n' */
    uint32_t params_len;      /* طولها بالبايت دون الصفر النهائي (0 = لا معاملات) */
} arp_exec_func_header_t;

int arp_aot_compile(
    const char *script,
    uint32_t    size,
    void      **out_binary,
    size_t     *out_size
);

int arp_aot_execute(
    const void *binary,
    size_t      size
);

int arp_aot_load(const char *path, void **out_binary, size_t *out_size);

int arp_aot_execute_library(const void *binary, size_t size);

#endif
