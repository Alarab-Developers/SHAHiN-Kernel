#ifndef ADDRESS_SPACE_H
#define ADDRESS_SPACE_H

#include "القلب/المكتبات/المكتبات.h"

typedef struct address_space {

    uint64_t *pml4;

} address_space_t;

void address_space_init(void);

address_space_t *address_space_create(void);

void address_space_destroy(address_space_t *space);

void address_space_switch(address_space_t *space);

address_space_t *address_space_current(void);

#endif
