#ifndef رياضيات_H
#define رياضيات_H

#include "../طبقة_الترجمة/شجرة.h"

ast_node_t *math_parse_ast(const char *expression, int *error);
ast_node_t *math_parse_expression(int *error);

int ast_evaluate_typed(ast_node_t *node, arp_value_t *result, int *error);

double math_calculate(const char *expression, int *error);

#endif
