#ifndef المتغيرات_H
#define المتغيرات_H

#include "../طبقة_الترجمة/شجرة.h"

typedef struct
{
    char        *name;
    arp_value_t  value;
    arp_type_t   declared_type;
    int          used;
} variable_t;

void variables_init(void);

int variable_declare(
    const char *name,
    arp_type_t  type,
    const arp_value_t *initial_value
);

int variable_get_value(const char *name, arp_value_t *out);
int variable_set_value(const char *name, const arp_value_t *value);
int variable_get_declared_type(const char *name, arp_type_t *out_type);

/* دوال مساعدة للطباعة / أدوات */
int variable_to_string(const char *name, char *buffer, int buffer_size);

#endif
