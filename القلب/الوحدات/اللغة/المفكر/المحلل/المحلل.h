#ifndef المفسر_H
#define المفسر_H

#include "مفاهيم_اللغة/الدوال.h"
#include "القلب/المكتبات/المكتبات.h"

int arp_parse_function(
    const char *header,
    const char **body,
    int line_count
);

int arp_execute_line(
    const char *line
);


int arp_execute_lines(
    const char lines[][ARP_MAX_LINE_LENGTH],
    int line_count
);

int parse_numeric_argument(
    const char **p,
    double *value,
    int last
);

#endif
