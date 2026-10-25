#include "مخزن_المتغيرات.h"
#include "محرك_العمليات/بوابة_العمليات.h"
#include "القلب/الوحدات/محمل_التطبيقات/سياق_اللغة.h"
#include "../ادوات/ادوات.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"

#define LOCALS_INIT 16

bytecode_vm_t *arp_get_active_vm(void)
{
    arp_context_t *ctx = arp_get_current_context();
    return ctx ? (bytecode_vm_t *)ctx->active_vm : NULL;
}

void arp_set_active_vm(bytecode_vm_t *vm)
{
    arp_context_t *ctx = arp_get_current_context();
    if (ctx) ctx->active_vm = vm;
}

static int vm_locals_grow(bytecode_vm_t *vm, int needed)
{
    if (vm->local_capacity >= needed) return 0;
    int nc = vm->local_capacity ? vm->local_capacity * 2 : LOCALS_INIT;
    while (nc < needed) nc *= 2;

    bytecode_local_t *nl = (bytecode_local_t *)
        memory_api.alloc(nc * sizeof(bytecode_local_t));
    if (!nl) return -1;

    if (vm->locals)
    {
        memcpy(nl, vm->locals, vm->local_count * sizeof(bytecode_local_t));
        memory_api.free(vm->locals);
    }
    vm->locals = nl;
    vm->local_capacity = nc;
    return 0;
}

void bytecode_vm_init(bytecode_vm_t *vm)
{
    if (!vm) return;
    vm->locals         = NULL;
    vm->local_count    = 0;
    vm->local_capacity = 0;
}

void bytecode_vm_free(bytecode_vm_t *vm)
{
    if (!vm) return;
    if (vm->locals)
    {
        for (int i = 0; i < vm->local_count; i++)
        {
            if (vm->locals[i].name) memory_api.free(vm->locals[i].name);
            arp_value_free(&vm->locals[i].value);
        }
        memory_api.free(vm->locals);
        vm->locals = NULL;
    }
    vm->local_count    = 0;
    vm->local_capacity = 0;
}

static int vm_find_local(bytecode_vm_t *vm, const char *name)
{
    if (!vm || !name) return -1;
    for (int i = 0; i < vm->local_count; i++)
        if (vm->locals[i].used && vm->locals[i].name &&
            strcmp(vm->locals[i].name, name) == 0)
            return i;
    return -1;
}

int bytecode_vm_set_local(bytecode_vm_t *vm, const char *name,
                          const arp_value_t *value)
{
    if (!vm || !name || !value) return -1;

    int index = vm_find_local(vm, name);
    if (index >= 0)
    {
        arp_value_free(&vm->locals[index].value);
        arp_value_init(&vm->locals[index].value, ARP_TYPE_UNKNOWN);
        return arp_value_copy(&vm->locals[index].value, value);
    }

    if (vm_locals_grow(vm, vm->local_count + 1) < 0) return -1;

    index = vm->local_count;
    vm->locals[index].used = 1;
    vm->locals[index].name = arp_strdup(name);
    if (!vm->locals[index].name) return -1;
    arp_value_init(&vm->locals[index].value, ARP_TYPE_UNKNOWN);
    if (arp_value_copy(&vm->locals[index].value, value) < 0) return -1;
    vm->local_count++;
    return 0;
}

int bytecode_vm_get_local(bytecode_vm_t *vm, const char *name,
                          arp_value_t *out)
{
    if (!vm || !name || !out) return -1;
    int index = vm_find_local(vm, name);
    if (index < 0) return -1;
    return arp_value_copy(out, &vm->locals[index].value);
}
