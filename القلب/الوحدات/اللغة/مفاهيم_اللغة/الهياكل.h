#ifndef الهياكل_H
#define الهياكل_H

#include "../طبقة_الترجمة/شجرة.h"

void structs_init(void);

int struct_register(
    const char *name,
    const char **field_names,
    const arp_value_t *defaults,   /* ← تغيير */
    int field_count
);

int struct_exists(const char *name);
int struct_field_count(const char *name);
const char *struct_field_name_at(const char *name, int index);
const arp_value_t *struct_field_default_at(const char *name, int index);  /* ← تغيير */

#endif
