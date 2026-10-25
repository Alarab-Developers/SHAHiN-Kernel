#include "الشروط.h"
#include "رياضيات.h"
#include "المتغيرات.h"
#include "../ادوات/ادوات.h"
#include "القلب/اختبار/لوحة_المفاتيح/شروط_لوحة_المفاتيح.h"
#include "../طبقة_الترجمة/شجرة.h"

static const char *find_matching_paren(const char *p)
{
    if (!p) return NULL;
    int depth = 0, in_string = 0;
    while (*p)
    {
        if (*p == '"') in_string = !in_string;
        if (!in_string)
        {
            if (*p == '(') depth++;
            else if (*p == ')') { depth--; if (depth == 0) return p; }
        }
        p++;
    }
    return NULL;
}

int condition_is_start(const char *line)
{
    if (!line) return 0;
    return starts_with(skip_spaces(line), "اذا(");
}

int condition_is_else(const char *line)
{
    if (!line) return 0;
    return strcmp(skip_spaces(line), "والا") == 0;
}

int condition_is_end(const char *line)
{
    if (!line) return 0;
    return strcmp(skip_spaces(line), "نهاية_الشرط") == 0;
}

static const char *find_condition_operator(const char *expr, int *op_len)
{
    if (!expr || !op_len) return NULL;
    int depth = 0, in_string = 0;
    const char *p = expr;
    while (*p)
    {
        if (*p == '"') { in_string = !in_string; p++; continue; }
        if (in_string) { p++; continue; }
        if (*p == '(') { depth++; p++; continue; }
        if (*p == ')') { if (depth > 0) depth--; p++; continue; }

        if (depth == 0)
        {
            if (strncmp(p, ">=", 2) == 0) { *op_len = 2; return p; }
            if (strncmp(p, "<=", 2) == 0) { *op_len = 2; return p; }
            if (strncmp(p, "!=", 2) == 0) { *op_len = 2; return p; }
            if (strncmp(p, "=!", 2) == 0) { *op_len = 2; return p; }
            if (strncmp(p, "==", 2) == 0) { *op_len = 2; return p; }
            if (strncmp(p, ">!", 2) == 0) { *op_len = 2; return p; }
            if (strncmp(p, "<!", 2) == 0) { *op_len = 2; return p; }
            if (*p == '=') { *op_len = 1; return p; }
            if (*p == '>') { *op_len = 1; return p; }
            if (*p == '<') { *op_len = 1; return p; }
        }
        p++;
    }
    return NULL;
}

/* تحويل رمز المقارنة النصّي إلى ثابت AST_CMP_* */
static int map_op_to_ast(const char *op, int op_len)
{
    if (op_len == 2)
    {
        if (op[0] == '>' && op[1] == '=') return AST_CMP_GTE;
        if (op[0] == '<' && op[1] == '=') return AST_CMP_LTE;
        if (op[0] == '!' && op[1] == '=') return AST_CMP_NEQ;
        if (op[0] == '=' && op[1] == '!') return AST_CMP_NEQ;
        if (op[0] == '=' && op[1] == '=') return AST_CMP_EQ;
        if (op[0] == '>' && op[1] == '!') return AST_CMP_GT;
        if (op[0] == '<' && op[1] == '!') return AST_CMP_LT;
        return -1;
    }
    if (op[0] == '=') return AST_CMP_EQ;
    if (op[0] == '>') return AST_CMP_GT;
    if (op[0] == '<') return AST_CMP_LT;
    return -1;
}

/*
 * هل التعبير يحتوي على && أو || خارج الأقواس والنصوص؟
 * نستخدمها لتفعيل مسار AST الكامل بدل التقسيم اليدوي على رمز مقارنة واحد.
 */
static int expr_has_logical(const char *expr)
{
    if (!expr) return 0;
    int depth = 0, in_string = 0;
    const char *p = expr;
    while (*p)
    {
        if (*p == '"') { in_string = !in_string; p++; continue; }
        if (in_string) { p++; continue; }
        if (*p == '(') { depth++; p++; continue; }
        if (*p == ')') { if (depth > 0) depth--; p++; continue; }
        if (depth == 0)
        {
            if (p[0] == '&' && p[1] == '&') return 1;
            if (p[0] == '|' && p[1] == '|') return 1;
        }
        p++;
    }
    return 0;
}

/*
 * condition_evaluate — تقييم تعبير شرط
 *
 * يُرجع 1 (صحيح) أو 0 (خطأ) أو -1 (فشل).
 *
 * يُدعم الآن:
 *   - مقارنات بسيطة:  اذا(س > 5)
 *   - && و ||:        اذا(س > 0 && ص < 10)
 *   - بدون مقارنة:    اذا(س)
 *   - لوحة_المفاتيح:  اذا(لوحة_المفاتيح(...))
 */
int condition_evaluate(const char *condition)
{
    if (!condition) return -1;
    const char *start = skip_spaces(condition);
    if (!starts_with(start, "اذا(")) return -1;

    const char *p = skip_spaces(start + strlen("اذا("));
    const char *close = find_matching_paren(start);
    if (!close) return -1;

    const char *after = skip_spaces(close + 1);
    if (*after != '\0') return -1;

    int length = close - p;
    if (length <= 0) return -1;

    char *expression = arp_strndup(p, length);
    if (!expression) return -1;

    /* حالة خاصة: لوحة_المفاتيح(...) */
    const char *keyboard = skip_spaces(expression);
    if (starts_with(keyboard, "لوحة_المفاتيح("))
    {
        int r = keyboard_condition(keyboard);
        memory_api.free(expression);
        return r;
    }

    /* ★ المسار الجديد: وجود && أو || → قيّم كتعبير AST كامل
     *   (هذا يغطي الحالات المختلطة مثل: س > 0 && ص < 10 || ع == 5) */
    if (expr_has_logical(expression))
    {
        int err = 0;
        ast_node_t *root = math_parse_ast(expression, &err);
        memory_api.free(expression);
        if (err || !root) { ast_free(root); return -1; }

        arp_value_t rv;
        arp_value_init(&rv, ARP_TYPE_UNKNOWN);
        int e2 = 0;
        int rc = ast_evaluate_typed(root, &rv, &e2);
        ast_free(root);

        if (rc < 0 || e2) { arp_value_free(&rv); return -1; }

        int truthy = arp_value_truthy(&rv);
        arp_value_free(&rv);
        return truthy;
    }

    /* ابحث عن رمز المقارنة */
    int op_len = 0;
    const char *op_pos = find_condition_operator(expression, &op_len);
    if (!op_pos || op_len == 0)
    {
        /* لا يوجد رمز مقارنة — قيّم التعبير ككل (منطقي/عددي) */
        int err = 0;
        ast_node_t *root = math_parse_ast(expression, &err);
        memory_api.free(expression);
        if (err || !root) { ast_free(root); return -1; }

        arp_value_t rv;
        arp_value_init(&rv, ARP_TYPE_UNKNOWN);
        int e2 = 0;
        int rc = ast_evaluate_typed(root, &rv, &e2);
        ast_free(root);

        if (rc < 0 || e2) { arp_value_free(&rv); return -1; }

        int truthy = arp_value_truthy(&rv);
        arp_value_free(&rv);
        return truthy;
    }

    int ast_op = map_op_to_ast(op_pos, op_len);
    if (ast_op < 0) { memory_api.free(expression); return -1; }

    /* فصل اليسار واليمين */
    char *op = arp_strndup(op_pos, op_len);
    if (!op) { memory_api.free(expression); return -1; }

    int left_len = op_pos - expression;
    if (left_len <= 0) {
        memory_api.free(op); memory_api.free(expression); return -1;
    }
    char *left = arp_strndup(expression, left_len);
    if (!left) {
        memory_api.free(op); memory_api.free(expression); return -1;
    }

    const char *rs = skip_spaces(op_pos + op_len);
    if (*rs == '\0') {
        memory_api.free(left); memory_api.free(op); memory_api.free(expression);
        return -1;
    }
    char *right = arp_strdup(rs);
    if (!right) {
        memory_api.free(left); memory_api.free(op); memory_api.free(expression);
        return -1;
    }

    memory_api.free(op);
    memory_api.free(expression);

    /* تقييم الطرفين نوعيًا */
    int eL = 0, eR = 0;
    ast_node_t *lr = math_parse_ast(left,  &eL);
    ast_node_t *rr = math_parse_ast(right, &eR);
    memory_api.free(left);
    memory_api.free(right);

    if (eL || !lr) { ast_free(lr); ast_free(rr); return -1; }
    if (eR || !rr) { ast_free(lr); ast_free(rr); return -1; }

    arp_value_t lv, rv;
    arp_value_init(&lv, ARP_TYPE_UNKNOWN);
    arp_value_init(&rv, ARP_TYPE_UNKNOWN);

    int el2 = 0, er2 = 0;
    if (ast_evaluate_typed(lr, &lv, &el2) < 0 || el2)
    {
        ast_free(lr); ast_free(rr);
        arp_value_free(&lv); arp_value_free(&rv);
        return -1;
    }
    if (ast_evaluate_typed(rr, &rv, &er2) < 0 || er2)
    {
        ast_free(lr); ast_free(rr);
        arp_value_free(&lv); arp_value_free(&rv);
        return -1;
    }

    /* مقارنة نوعية موحّدة */
    int cmp_out = 0;
    int rc = arp_value_cmp(ast_op, &lv, &rv, &cmp_out);

    ast_free(lr);
    ast_free(rr);
    arp_value_free(&lv);
    arp_value_free(&rv);

    if (rc < 0) return -1;
    return cmp_out;
}
