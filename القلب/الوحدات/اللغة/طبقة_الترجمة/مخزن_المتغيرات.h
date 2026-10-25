#ifndef آلة_بايت_كود_H
#define آلة_بايت_كود_H

#include "شجرة.h"

typedef struct
{
    char        *name;
    arp_value_t  value;   /* ← تغيير: كان ast_value_t */
    int          used;
} bytecode_local_t;

typedef struct
{
    bytecode_local_t *locals;
    int               local_count;
    int               local_capacity;
} bytecode_vm_t;

bytecode_vm_t *arp_get_active_vm(void);
void           arp_set_active_vm(bytecode_vm_t *vm);

void bytecode_vm_init(bytecode_vm_t *vm);
void bytecode_vm_free(bytecode_vm_t *vm);
int  bytecode_vm_set_local(bytecode_vm_t *vm, const char *name,
                           const arp_value_t *value);
int  bytecode_vm_get_local(bytecode_vm_t *vm, const char *name,
                           arp_value_t *out);

#endif
