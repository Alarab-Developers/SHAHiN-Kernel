#ifndef الحلقات_H
#define الحلقات_H
#include "الدوال.h"
#include "القلب/المكتبات/المكتبات.h"

int loop_is_start(
    const char *line
);


int loop_is_end(
    const char *line
);


int loop_execute(
    const char lines[][ARP_MAX_LINE_LENGTH],
    int line_count
);


#endif
