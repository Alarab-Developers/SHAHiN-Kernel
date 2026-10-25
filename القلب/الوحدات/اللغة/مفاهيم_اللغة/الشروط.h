#ifndef الشروط_H
#define الشروط_H

#include "القلب/المكتبات/المكتبات.h"

int condition_evaluate(const char *condition);

int condition_is_start(const char *line);

int condition_is_else(const char *line);

int condition_is_end(const char *line);

#endif
