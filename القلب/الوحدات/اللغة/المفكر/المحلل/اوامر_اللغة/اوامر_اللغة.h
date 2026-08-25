#ifndef اوامر_اللغة_H
#define اوامر_اللغة_H

int parse_variable_declaration(
    const char *p
);

int parse_library_import(
    const char *p
);

int parse_app(
    const char *p
);

int parse_calculate(
    const char *p
);

int parse_rect(
    const char *p
);

int parse_background(
    const char *p
);

#endif
