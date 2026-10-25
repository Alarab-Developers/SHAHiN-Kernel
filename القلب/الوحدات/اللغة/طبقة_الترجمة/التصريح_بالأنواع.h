#ifndef التصريح_بالأنواع_H
#define التصريح_بالأنواع_H

#include "شجرة.h"

/* هل السطر يبدأ بتصريح نوع عادي؟ "صحيح العمر = 12" */
int declaration_is_start(const char *line, arp_type_t *out_type,
                         char **out_var_name, const char **out_rhs);

/* هل السطر تصريح مصفوفة؟ "صحيح مدونة(4) = {...}" */
int declaration_is_array_start(const char *line,
                               arp_type_t *out_type,
                               char **out_var_name,
                               int *out_dims, int *out_ndims,
                               const char **out_rhs);

void declaration_free(char *var_name);

#endif
