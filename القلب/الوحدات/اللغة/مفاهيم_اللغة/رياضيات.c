#include "رياضيات.h"
#include "المتغيرات.h"
#include "الدوال.h"
#include "../ادوات/ادوات.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "../طبقة_الترجمة/المحلل.h"

static const char *current;

static ast_node_t *parse_comparison(int *error);
static ast_node_t *parse_logical_or(int *error);

/* =========================================================
 * أرقام عربية-هندية
 * ========================================================= */
static int utf8_arabic_digit(const char *p, char *ascii_out)
{
    if (!p || !p[0] || !p[1]) return 0;
    unsigned char b0 = (unsigned char)p[0];
    unsigned char b1 = (unsigned char)p[1];
    if (b0 == 0xD9 && b1 >= 0xA0 && b1 <= 0xA9) {
        if (ascii_out) *ascii_out = (char)('0' + (b1 - 0xA0));
        return 2;
    }
    return 0;
}

static void skip_math_spaces(void)
{
    while (*current == ' ' || *current == '\t') current++;
}

static int is_digit(char c) { return (c >= '0' && c <= '9'); }

/* =========================================================
 * parse_number
 * ========================================================= */
static int parse_number_value(int *error, arp_value_t *out)
{
    skip_math_spaces();

    if (current[0] == '0' && (current[1] == 'x' || current[1] == 'X'))
    {
        current += 2;
        uint64_t value = 0;
        int digits = 0;
        while (*current)
        {
            char c = *current;
            uint64_t digit;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else break;
            value = (value << 4) | digit;
            digits++;
            current++;
        }
        if (digits == 0) { *error = 1; return -1; }
        arp_value_make_int(out, (int64_t)value);
        return 0;
    }

    int64_t int_part = 0;
    int digits = 0;
    int has_fraction = 0;

    while (*current)
    {
        char c = *current;
        int digit = -1;
        int adv = 1;

        if (c >= '0' && c <= '9') { digit = c - '0'; }
        else
        {
            char ascii;
            int n = utf8_arabic_digit(current, &ascii);
            if (n) { digit = ascii - '0'; adv = n; }
        }

        if (digit < 0) break;
        int_part = int_part * 10 + digit;
        current += adv;
        digits++;
    }

    double fraction = 0.0;
    if (*current == '.')
    {
        has_fraction = 1;
        current++;
        double divisor = 10.0;
        while (*current)
        {
            char c = *current;
            int digit = -1;
            int adv = 1;

            if (c >= '0' && c <= '9') { digit = c - '0'; }
            else
            {
                char ascii;
                int n = utf8_arabic_digit(current, &ascii);
                if (n) { digit = ascii - '0'; adv = n; }
            }

            if (digit < 0) break;
            fraction += digit / divisor;
            divisor *= 10.0;
            current += adv;
            digits++;
        }
    }

    if (digits == 0) { *error = 1; return -1; }

    if (has_fraction)
        arp_value_make_double(out, (double)int_part + fraction);
    else
        arp_value_make_int(out, int_part);
    return 0;
}

/* =========================================================
 * parse_variable_name_dyn
 * ========================================================= */
static char *parse_variable_name_dyn(void)
{
    int len = 0, cap = 32;
    char *name = (char *)memory_api.alloc(cap);
    if (!name) return NULL;

    skip_math_spaces();
    while (*current &&
           *current != ' ' && *current != '\t' &&
           *current != '+' && *current != '-' &&
           *current != '*' && *current != '/' &&
           *current != '%' &&
           *current != '(' && *current != ')' &&
           *current != ',' && *current != '>' &&
           *current != '<' && *current != '=' &&
           *current != '!' && *current != '&' &&
           *current != '|' && *current != '^' &&
           *current != '{' && *current != '}')
    {
        if (len + 1 >= cap)
        {
            cap *= 2;
            char *nn = (char *)memory_api.alloc(cap);
            if (!nn) { memory_api.free(name); return NULL; }
            memcpy(nn, name, len);
            memory_api.free(name);
            name = nn;
        }
        name[len++] = *current++;
    }
    if (len == 0) { memory_api.free(name); return NULL; }
    name[len] = '\0';
    return name;
}

/* =========================================================
 * parse_function_call — كل معامل يُحلَّل بـ parse_comparison
 * ========================================================= */
static ast_node_t *parse_function_call(const char *name, int *error)
{
    ast_node_t **args = NULL;
    int arg_count = 0;

    if (*current != '(') { *error = 1; return NULL; }
    current++;
    skip_math_spaces();

    if (*current == ')')
    {
        current++;
        return ast_function_call(name, NULL, 0);
    }

    while (!*error)
    {
        ast_node_t *arg = parse_comparison(error);
        if (*error || !arg)
        {
            ast_free(arg);
            for (int i = 0; i < arg_count; i++) ast_free(args[i]);
            if (args) memory_api.free(args);
            *error = 1;
            return NULL;
        }

        ast_node_t **na = (ast_node_t **)memory_api.alloc((arg_count + 1) * sizeof(ast_node_t *));
        if (!na)
        {
            ast_free(arg);
            for (int i = 0; i < arg_count; i++) ast_free(args[i]);
            if (args) memory_api.free(args);
            *error = 1;
            return NULL;
        }
        for (int i = 0; i < arg_count; i++) na[i] = args[i];
        na[arg_count++] = arg;
        if (args) memory_api.free(args);
        args = na;

        skip_math_spaces();
        if (*current == ')') { current++; break; }
        if (*current != ',')
        {
            *error = 1;
            for (int i = 0; i < arg_count; i++) ast_free(args[i]);
            memory_api.free(args);
            return NULL;
        }
        current++;
        skip_math_spaces();
        if (*current == ')')
        {
            *error = 1;
            for (int i = 0; i < arg_count; i++) ast_free(args[i]);
            memory_api.free(args);
            return NULL;
        }
    }

    if (*error)
    {
        if (args)
        {
            for (int i = 0; i < arg_count; i++) ast_free(args[i]);
            memory_api.free(args);
        }
        return NULL;
    }

    ast_node_t *node = ast_function_call(name, args, arg_count);
    if (!node)
    {
        for (int i = 0; i < arg_count; i++) ast_free(args[i]);
        if (args) memory_api.free(args);
        *error = 1;
        return NULL;
    }
    return node;
}

/* =========================================================
 * parse_array_literal — {a, b, c} أو {{...}, {...}}
 * ========================================================= */
static ast_node_t *parse_array_literal(int *error)
{
    if (*current != '{') { *error = 1; return NULL; }
    current++;
    skip_math_spaces();

    if (*current == '}')
    {
        current++;
        return ast_array_lit(NULL, 0);
    }

    ast_node_t **elems = NULL;
    int n = 0;

    while (!*error)
    {
        ast_node_t *e = parse_comparison(error);
        if (*error || !e)
        {
            ast_free(e);
            for (int i = 0; i < n; i++) ast_free(elems[i]);
            if (elems) memory_api.free(elems);
            *error = 1;
            return NULL;
        }

        ast_node_t **na = (ast_node_t **)memory_api.alloc((n + 1) * sizeof(ast_node_t *));
        if (!na)
        {
            ast_free(e);
            for (int i = 0; i < n; i++) ast_free(elems[i]);
            if (elems) memory_api.free(elems);
            *error = 1;
            return NULL;
        }
        for (int i = 0; i < n; i++) na[i] = elems[i];
        na[n++] = e;
        if (elems) memory_api.free(elems);
        elems = na;

        skip_math_spaces();
        if (*current == '}') { current++; break; }
        if (*current != ',')
        {
            *error = 1;
            for (int i = 0; i < n; i++) ast_free(elems[i]);
            memory_api.free(elems);
            return NULL;
        }
        current++;
        skip_math_spaces();
    }

    return ast_array_lit(elems, n);
}

/* =========================================================
 * parse_factor
 * ========================================================= */
static ast_node_t *parse_factor(int *error)
{
    skip_math_spaces();

    if (*current == '(')
    {
        current++;
        ast_node_t *node = parse_comparison(error);
        if (*error) { ast_free(node); return NULL; }
        skip_math_spaces();
        if (*current != ')') { *error = 1; ast_free(node); return NULL; }
        current++;
        return node;
    }

    if (*current == '{')
        return parse_array_literal(error);

    if (*current == '-')
    {
        current++;
        ast_node_t *operand = parse_factor(error);
        if (*error || !operand) { *error = 1; ast_free(operand); return NULL; }
        ast_node_t *node = ast_unary(AST_NEG, operand);
        if (!node) { *error = 1; ast_free(operand); return NULL; }
        return node;
    }

    if (*current == '"')
    {
        current++;
        int len = 0, cap = 32;
        char *text = (char *)memory_api.alloc(cap);
        if (!text) { *error = 1; return NULL; }

        while (*current && *current != '"')
        {
            if (len + 1 >= cap)
            {
                cap *= 2;
                char *nt = (char *)memory_api.alloc(cap);
                if (!nt) { memory_api.free(text); *error = 1; return NULL; }
                memcpy(nt, text, len);
                memory_api.free(text);
                text = nt;
            }
            text[len++] = *current++;
        }

        if (*current != '"') { memory_api.free(text); *error = 1; return NULL; }
        current++;
        text[len] = '\0';

        ast_node_t *node = ast_string(text);
        memory_api.free(text);
        return node;
    }

    {
        char dummy;
        if (is_digit(*current) || *current == '.' ||
            utf8_arabic_digit(current, &dummy))
        {
            arp_value_t num;
            arp_value_init(&num, ARP_TYPE_UNKNOWN);
            if (parse_number_value(error, &num) < 0) return NULL;

            ast_node_t *node;
            if (num.type == ARP_TYPE_INT)
                node = ast_number((double)num.as.i64);
            else
                node = ast_number(num.as.f64);
            arp_value_free(&num);
            return node;
        }
    }

    if (*current)
    {
        char *name = parse_variable_name_dyn();
        if (!name) { *error = 1; return NULL; }

        skip_math_spaces();

        ast_node_t *node;
        if (*current == '(')
            node = parse_function_call(name, error);
        else
            node = ast_variable(name);

        memory_api.free(name);
        return node;
    }

    *error = 1;
    return NULL;
}

/* =========================================================
 * parse_term / math_parse_expression
 * ========================================================= */
static ast_node_t *parse_term(int *error)
{
    ast_node_t *left = parse_factor(error);
    if (*error) { ast_free(left); return NULL; }

    while (!*error)
    {
        skip_math_spaces();
        if (*current == '*')
        {
            current++;
            ast_node_t *right = parse_factor(error);
            if (*error || !right) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            ast_node_t *nn = ast_binary(AST_MUL, left, right);
            if (!nn) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            left = nn;
        }
        else if (*current == '/')
        {
            current++;
            ast_node_t *right = parse_factor(error);
            if (*error || !right) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            ast_node_t *nn = ast_binary(AST_DIV, left, right);
            if (!nn) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            left = nn;
        }
        else if (*current == '%')
        {
            current++;
            ast_node_t *right = parse_factor(error);
            if (*error || !right) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            ast_node_t *nn = ast_binary(AST_MOD, left, right);
            if (!nn) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            left = nn;
        }
        else break;
    }
    return left;
}

ast_node_t *math_parse_expression(int *error)
{
    ast_node_t *left = parse_term(error);
    if (*error) { ast_free(left); return NULL; }

    while (!*error)
    {
        skip_math_spaces();
        if (*current == '+')
        {
            current++;
            ast_node_t *right = parse_term(error);
            if (*error || !right) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            ast_node_t *nn = ast_binary(AST_ADD, left, right);
            if (!nn) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            left = nn;
        }
        else if (*current == '-')
        {
            current++;
            ast_node_t *right = parse_term(error);
            if (*error || !right) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            ast_node_t *nn = ast_binary(AST_SUB, left, right);
            if (!nn) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            left = nn;
        }
        else break;
    }
    return left;
}

/* =========================================================
 * parse_shift / parse_bitand / parse_bitxor / parse_bitor
 * ========================================================= */
static ast_node_t *parse_shift(int *error)
{
    ast_node_t *left = math_parse_expression(error);
    if (*error) { ast_free(left); return NULL; }

    while (!*error)
    {
        skip_math_spaces();
        if (current[0] == '<' && current[1] == '<')
        {
            current += 2;
            ast_node_t *right = math_parse_expression(error);
            if (*error || !right) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            ast_node_t *nn = ast_binary(AST_SHL, left, right);
            if (!nn) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            left = nn;
        }
        else if (current[0] == '>' && current[1] == '>')
        {
            current += 2;
            ast_node_t *right = math_parse_expression(error);
            if (*error || !right) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            ast_node_t *nn = ast_binary(AST_SHR, left, right);
            if (!nn) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            left = nn;
        }
        else break;
    }
    return left;
}

static ast_node_t *parse_bitand(int *error)
{
    ast_node_t *left = parse_shift(error);
    if (*error) { ast_free(left); return NULL; }

    while (!*error)
    {
        skip_math_spaces();
        if (*current == '&' && current[1] != '&')
        {
            current++;
            ast_node_t *right = parse_shift(error);
            if (*error || !right) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            ast_node_t *nn = ast_binary(AST_BIT_AND, left, right);
            if (!nn) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            left = nn;
        }
        else break;
    }
    return left;
}

static ast_node_t *parse_bitxor(int *error)
{
    ast_node_t *left = parse_bitand(error);
    if (*error) { ast_free(left); return NULL; }

    while (!*error)
    {
        skip_math_spaces();
        if (*current == '^')
        {
            current++;
            ast_node_t *right = parse_bitand(error);
            if (*error || !right) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            ast_node_t *nn = ast_binary(AST_BIT_XOR, left, right);
            if (!nn) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            left = nn;
        }
        else break;
    }
    return left;
}

static ast_node_t *parse_bitor(int *error)
{
    ast_node_t *left = parse_bitxor(error);
    if (*error) { ast_free(left); return NULL; }

    while (!*error)
    {
        skip_math_spaces();
        if (*current == '|' && current[1] != '|')
        {
            current++;
            ast_node_t *right = parse_bitxor(error);
            if (*error || !right) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            ast_node_t *nn = ast_binary(AST_BIT_OR, left, right);
            if (!nn) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            left = nn;
        }
        else break;
    }
    return left;
}

/* =========================================================
 * parse_logical_and / parse_logical_or
 * ========================================================= */
static ast_node_t *parse_logical_and(int *error)
{
    ast_node_t *left = parse_bitor(error);
    if (*error) { ast_free(left); return NULL; }

    while (!*error)
    {
        skip_math_spaces();
        if (current[0] == '&' && current[1] == '&')
        {
            current += 2;
            ast_node_t *right = parse_bitor(error);
            if (*error || !right) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            ast_node_t *nn = ast_binary(AST_AND, left, right);
            if (!nn) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            left = nn;
        }
        else break;
    }
    return left;
}

static ast_node_t *parse_logical_or(int *error)
{
    ast_node_t *left = parse_logical_and(error);
    if (*error) { ast_free(left); return NULL; }

    while (!*error)
    {
        skip_math_spaces();
        if (current[0] == '|' && current[1] == '|')
        {
            current += 2;
            ast_node_t *right = parse_logical_and(error);
            if (*error || !right) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            ast_node_t *nn = ast_binary(AST_OR, left, right);
            if (!nn) { *error = 1; ast_free(left); ast_free(right); return NULL; }
            left = nn;
        }
        else break;
    }
    return left;
}

/* =========================================================
 * parse_comparison
 * ========================================================= */
static ast_node_t *parse_comparison(int *error)
{
    ast_node_t *left = parse_logical_or(error);
    if (*error) { ast_free(left); return NULL; }

    skip_math_spaces();

    ast_node_type_t cmp_type = AST_NUMBER;
    int matched = 0;

    if      (current[0] == '=' && current[1] == '=') { cmp_type = AST_CMP_EQ;  current += 2; matched = 1; }
    else if (current[0] == '!' && current[1] == '=') { cmp_type = AST_CMP_NEQ; current += 2; matched = 1; }
    else if (current[0] == '=' && current[1] == '!') { cmp_type = AST_CMP_NEQ; current += 2; matched = 1; }
    else if (current[0] == '>' && current[1] == '=') { cmp_type = AST_CMP_GTE; current += 2; matched = 1; }
    else if (current[0] == '<' && current[1] == '=') { cmp_type = AST_CMP_LTE; current += 2; matched = 1; }
    else if (current[0] == '>' && current[1] == '!') { cmp_type = AST_CMP_GT;  current += 2; matched = 1; }
    else if (current[0] == '<' && current[1] == '!') { cmp_type = AST_CMP_LT;  current += 2; matched = 1; }
    else if (current[0] == '>') { cmp_type = AST_CMP_GT; current += 1; matched = 1; }
    else if (current[0] == '<') { cmp_type = AST_CMP_LT; current += 1; matched = 1; }

    if (!matched) return left;

    skip_math_spaces();
    ast_node_t *right = parse_logical_or(error);
    if (*error || !right) { ast_free(left); ast_free(right); return NULL; }

    ast_node_t *node = ast_binary(cmp_type, left, right);
    if (!node) { *error = 1; ast_free(left); ast_free(right); return NULL; }
    return node;
}

/* =========================================================
 * math_parse_ast
 * ========================================================= */
ast_node_t *math_parse_ast(const char *expression, int *error)
{
    if (!expression || !error) return NULL;
    *error = 0;
    current = expression;

    ast_node_t *root = parse_comparison(error);
    if (*error) { ast_free(root); return NULL; }

    skip_math_spaces();
    if (*current != '\0') { *error = 1; ast_free(root); return NULL; }
    return root;
}

/* =========================================================
 * التقييم النوعي
 * ========================================================= */
int ast_evaluate_typed(ast_node_t *node, arp_value_t *result, int *error)
{
    if (!node || !result || !error) { if (error) *error = 1; return -1; }
    *error = 0;

    arp_value_free(result);

    switch (node->type)
    {
        case AST_NUMBER:
        {
            double d = node->number;
            if (d == (double)(int64_t)d &&
                d >= -9223372036854775808.0 &&
                d <  9223372036854775808.0)
                arp_value_make_int(result, (int64_t)d);
            else
                arp_value_make_double(result, d);
            return 0;
        }

        case AST_STRING:
            return arp_value_make_text(result, node->text ? node->text : "", -1);

        case AST_VARIABLE:
        {
            if (variable_get_value(node->name, result) < 0)
            { *error = 1; return -1; }
            return 0;
        }

        case AST_FUNCTION_CALL:
        {
            const char *name = node->function_call.name;

            /* 1) دالة مسجّلة → السلوك القديم */
            if (function_find(name))
            {
                int count = node->function_call.arg_count;
                arp_value_t *values = NULL;
                if (count > 0)
                {
                    values = (arp_value_t *)memory_api.alloc(count * sizeof(arp_value_t));
                    if (!values) { *error = 1; return -1; }
                    for (int i = 0; i < count; i++)
                    {
                        arp_value_init(&values[i], ARP_TYPE_UNKNOWN);
                        if (ast_evaluate_typed(node->function_call.args[i], &values[i], error) < 0)
                        {
                            for (int j = 0; j <= i; j++) arp_value_free(&values[j]);
                            memory_api.free(values);
                            return -1;
                        }
                    }
                }
                arp_value_t rv;
                arp_value_init(&rv, ARP_TYPE_UNKNOWN);
                int rc = arp_invoke_function_typed(name, values, count, &rv);
                if (values)
                {
                    for (int i = 0; i < count; i++) arp_value_free(&values[i]);
                    memory_api.free(values);
                }
                if (rc < 0) { arp_value_free(&rv); *error = 1; return -1; }
                arp_value_free(result);
                *result = rv;
                return 0;
            }

            /* 2) متغير مصفوفة → فهرس */
            arp_value_t holder;
            arp_value_init(&holder, ARP_TYPE_UNKNOWN);
            if (variable_get_value(name, &holder) == 0 &&
                holder.type == ARP_TYPE_ARRAY)
            {
                int nidx = node->function_call.arg_count;
                if (nidx < 1 || nidx > 8) { arp_value_free(&holder); *error = 1; return -1; }

                int idx[8];
                for (int i = 0; i < nidx; i++)
                {
                    arp_value_t iv;
                    arp_value_init(&iv, ARP_TYPE_UNKNOWN);
                    if (ast_evaluate_typed(node->function_call.args[i], &iv, error) < 0)
                    { arp_value_free(&holder); arp_value_free(&iv); return -1; }
                    int64_t x;
                    if (arp_value_to_i64(&iv, &x) < 0)
                    { arp_value_free(&holder); arp_value_free(&iv); *error = 1; return -1; }
                    arp_value_free(&iv);
                    idx[i] = (int)x;
                }

                int rc = arp_array_get(holder.as.array, idx, nidx, result);
                arp_value_free(&holder);
                if (rc < 0) { *error = 1; return -1; }
                return 0;
            }
            arp_value_free(&holder);

            *error = 1;
            return -1;
        }

        case AST_ARRAY_LIT:
        {
            /* مصفوفة حرفية خارج سياق تصريح — غير مدعومة */
            *error = 1;
            return -1;
        }

        case AST_CMP_EQ: case AST_CMP_NEQ:
        case AST_CMP_LT: case AST_CMP_LTE:
        case AST_CMP_GT: case AST_CMP_GTE:
        {
            arp_value_t left, right;
            arp_value_init(&left,  ARP_TYPE_UNKNOWN);
            arp_value_init(&right, ARP_TYPE_UNKNOWN);

            if (ast_evaluate_typed(node->binary.left,  &left,  error) < 0)
            { arp_value_free(&left); return -1; }
            if (ast_evaluate_typed(node->binary.right, &right, error) < 0)
            { arp_value_free(&left); arp_value_free(&right); return -1; }

            int cmp_out = 0;
            int rc = arp_value_cmp(node->type, &left, &right, &cmp_out);

            arp_value_free(&left);
            arp_value_free(&right);

            if (rc < 0) { *error = 1; return -1; }
            arp_value_make_bool(result, cmp_out);
            return 0;
        }

        case AST_NEG:
        {
            arp_value_t operand;
            arp_value_init(&operand, ARP_TYPE_UNKNOWN);
            if (ast_evaluate_typed(node->unary.operand, &operand, error) < 0)
            { arp_value_free(&operand); return -1; }

            int rc = arp_value_neg(&operand, result);
            arp_value_free(&operand);

            if (rc < 0) { *error = 1; return -1; }
            return 0;
        }

        case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV:
        case AST_MOD: case AST_SHL: case AST_SHR:
        case AST_BIT_AND: case AST_BIT_OR: case AST_BIT_XOR:
        {
            arp_value_t left, right;
            arp_value_init(&left,  ARP_TYPE_UNKNOWN);
            arp_value_init(&right, ARP_TYPE_UNKNOWN);

            if (ast_evaluate_typed(node->binary.left,  &left,  error) < 0)
            { arp_value_free(&left); return -1; }
            if (ast_evaluate_typed(node->binary.right, &right, error) < 0)
            { arp_value_free(&left); arp_value_free(&right); return -1; }

            int rc = -1;
            switch (node->type) {
                case AST_ADD:     rc = arp_value_add(&left, &right, result); break;
                case AST_SUB:     rc = arp_value_sub(&left, &right, result); break;
                case AST_MUL:     rc = arp_value_mul(&left, &right, result); break;
                case AST_DIV:     rc = arp_value_div(&left, &right, result); break;
                case AST_MOD:     rc = arp_value_mod(&left, &right, result); break;
                case AST_SHL:     rc = arp_value_shl(&left, &right, result); break;
                case AST_SHR:     rc = arp_value_shr(&left, &right, result); break;
                case AST_BIT_AND: rc = arp_value_bit_and(&left, &right, result); break;
                case AST_BIT_OR:  rc = arp_value_bit_or (&left, &right, result); break;
                case AST_BIT_XOR: rc = arp_value_bit_xor(&left, &right, result); break;
                default: break;
            }

            arp_value_free(&left);
            arp_value_free(&right);

            if (rc < 0) { *error = 1; return -1; }
            return 0;
        }

        case AST_ASSIGN:
        {
            arp_value_t rv;
            arp_value_init(&rv, ARP_TYPE_UNKNOWN);
            if (ast_evaluate_typed(node->assignment.value, &rv, error) < 0)
            { arp_value_free(&rv); return -1; }

            int rc = variable_set_value(node->assignment.name, &rv);
            if (rc < 0) { arp_value_free(&rv); *error = 1; return -1; }

            arp_value_free(result);
            *result = rv;
            return 0;
        }

        case AST_AND:
        {
            arp_value_t left, right;
            arp_value_init(&left,  ARP_TYPE_UNKNOWN);
            arp_value_init(&right, ARP_TYPE_UNKNOWN);

            if (ast_evaluate_typed(node->binary.left, &left, error) < 0)
            { arp_value_free(&left); return -1; }

            int truth = arp_value_truthy(&left);
            arp_value_free(&left);

            if (truth == 0) {
                arp_value_make_bool(result, 0);
                return 0;
            }

            if (ast_evaluate_typed(node->binary.right, &right, error) < 0)
            { arp_value_free(&right); return -1; }

            truth = arp_value_truthy(&right);
            arp_value_free(&right);

            arp_value_make_bool(result, truth ? 1 : 0);
            return 0;
        }

        case AST_OR:
        {
            arp_value_t left, right;
            arp_value_init(&left,  ARP_TYPE_UNKNOWN);
            arp_value_init(&right, ARP_TYPE_UNKNOWN);

            if (ast_evaluate_typed(node->binary.left, &left, error) < 0)
            { arp_value_free(&left); return -1; }

            int truth = arp_value_truthy(&left);
            arp_value_free(&left);

            if (truth != 0) {
                arp_value_make_bool(result, 1);
                return 0;
            }

            if (ast_evaluate_typed(node->binary.right, &right, error) < 0)
            { arp_value_free(&right); return -1; }

            truth = arp_value_truthy(&right);
            arp_value_free(&right);

            arp_value_make_bool(result, truth ? 1 : 0);
            return 0;
        }

        default:
            *error = 1;
            return -1;
    }
}

/* =========================================================
 * math_calculate
 * ========================================================= */
double math_calculate(const char *expression, int *error)
{
    if (!expression || !error) { if (error) *error = 1; return 0; }
    *error = 0;

    ast_node_t *root = math_parse_ast(expression, error);
    if (*error || !root) return 0;

    arp_value_t result;
    arp_value_init(&result, ARP_TYPE_UNKNOWN);
    if (ast_evaluate_typed(root, &result, error) < 0)
    { ast_free(root); arp_value_free(&result); return 0; }
    ast_free(root);

    double out = 0;
    if (arp_value_to_f64(&result, &out) < 0) *error = 1;
    arp_value_free(&result);
    return out;
}
