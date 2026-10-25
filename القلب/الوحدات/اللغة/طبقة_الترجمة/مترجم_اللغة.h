#ifndef مترجم_بايت_كود_H
#define مترجم_بايت_كود_H

#include "شجرة.h"
#include "القلب/المكتبات/المكتبات.h"

int machine_ast_is_jit_supported(const ast_node_t *node);

typedef struct {
    uint32_t offset;
    uint32_t symbol;
} jit_reloc_t;

int machine_jit_compile_function(const char **lines, int line_count,
                                 void **out_code, size_t *out_size);

int machine_jit_compile_function_reloc(const char **lines, int line_count,
                                       void **out_code, size_t *out_size,
                                       jit_reloc_t **out_relocs,
                                       uint32_t *out_reloc_count);

void    *machine_jit_resolve_symbol(uint32_t symbol);
uint32_t machine_jit_symbol_count(void);

typedef struct
{
    arp_type_t type;
    double     numeric;
    char      *text;
} jit_result_t;

int  machine_jit_execute_typed(void *code, jit_result_t *out);
void jit_result_free(jit_result_t *r);
int  machine_jit_take_text_result(char **out_text);

#endif
