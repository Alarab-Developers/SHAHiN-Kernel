#ifndef اوامر_اللغة_H
#define اوامر_اللغة_H

int parse_assignment(const char *p);
int parse_library_import(const char *p);
int parse_app(const char *p);
int parse_calculate(const char *p);
int parse_rect(const char *p);
int parse_triangle(const char *p);
int parse_present(const char *p);
int parse_background(const char *p);
int parse_read_hardware(const char *p);
int parse_memory_write(const char *p);
int parse_memory_read(const char *p);
int parse_wait(const char *p);
int parse_print(const char *p);
int parse_input(const char *p);
int parse_return(const char *p);

#endif
