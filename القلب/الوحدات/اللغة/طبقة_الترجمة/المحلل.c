#include "المحلل.h"
#include "../مفاهيم_اللغة/الدوال.h"
#include "مخزن_المتغيرات.h"
#include "مترجم_اللغة.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "../ادوات/ادوات.h"

int arp_invoke_function_typed(
    const char *name,
    const arp_value_t *arguments,
    int argument_count,
    arp_value_t *result)
{
    if (!name || !result || argument_count < 0) return -1;

    arp_function_t *f = function_find(name);
    if (!f) return -1;
    if (argument_count != f->parameter_count) return -1;
    if (!f->jit_code) return -1;

    bytecode_vm_t *vm = (bytecode_vm_t *)memory_api.alloc(sizeof(bytecode_vm_t));
    if (!vm) return -1;
    bytecode_vm_init(vm);

    for (int i = 0; i < f->parameter_count; i++)
    {
        if (bytecode_vm_set_local(vm, f->parameters[i], &arguments[i]) < 0)
        {
            bytecode_vm_free(vm);
            memory_api.free(vm);
            return -1;
        }
    }

    bytecode_vm_t *prev_vm = arp_get_active_vm();
    arp_set_active_vm(vm);

    jit_result_t jr;
    jr.type = ARP_TYPE_UNKNOWN;
    jr.numeric = 0.0;
    jr.text = NULL;

    if (machine_jit_execute_typed(f->jit_code, &jr) != 0)
    {
        arp_set_active_vm(prev_vm);
        bytecode_vm_free(vm);
        memory_api.free(vm);
        return -1;
    }

    arp_value_free(result);

    switch (jr.type)
    {
        case ARP_TYPE_TEXT:
            result->type = ARP_TYPE_TEXT;
            result->as.text = jr.text;
            jr.text = NULL;
            break;
        case ARP_TYPE_INT:    arp_value_make_int(result, (int64_t)jr.numeric); break;
        case ARP_TYPE_FLOAT:  arp_value_make_float(result, (float)jr.numeric); break;
        case ARP_TYPE_DOUBLE: arp_value_make_double(result, jr.numeric); break;
        case ARP_TYPE_CHAR:   arp_value_make_char(result, (uint32_t)jr.numeric); break;
        case ARP_TYPE_BOOL:   arp_value_make_bool(result, (int)jr.numeric); break;
        case ARP_TYPE_PTR:    arp_value_make_ptr(result, (uintptr_t)(uint64_t)jr.numeric); break;
        default:              arp_value_make_double(result, jr.numeric); break;
    }
    jit_result_free(&jr);

    arp_set_active_vm(prev_vm);
    bytecode_vm_free(vm);
    memory_api.free(vm);
    return 0;
}
