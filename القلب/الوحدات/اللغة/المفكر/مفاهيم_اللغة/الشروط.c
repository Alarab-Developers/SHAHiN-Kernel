#include "الشروط.h"
#include "رياضيات.h"
#include "المتغيرات.h"
#include "../../ادوات/ادوات.h"
#include "القلب/اختبار/لوحة_المفاتيح/شروط_لوحة_المفاتيح.h"


/* ================================================================ */
/* البحث عن القوس المطابق                                           */
/* ================================================================ */

static const char *find_matching_paren(const char *p) {
    if (!p)
        return NULL;

    int depth = 0;
    while (*p) {
        if (*p == '(') {
            depth++;
        } else if (*p == ')') {
            depth--;
            if (depth == 0)
                return p;
        }
        p++;
    }
    return NULL;
}

/* ================================================================ */
/* التحقق من بداية الشرط                                             */
/* ================================================================ */

int condition_is_start(const char *line) {
    if (!line)
        return 0;
    const char *p = skip_spaces(line);
    return starts_with(p, "اذا(");
}

/* ================================================================ */
/* التحقق من والا                                                     */
/* ================================================================ */

int condition_is_else(const char *line) {
    if (!line)
        return 0;
    const char *p = skip_spaces(line);
    return strcmp(p, "والا") == 0;
}

/* ================================================================ */
/* التحقق من نهاية الشرط                                             */
/* ================================================================ */

int condition_is_end(const char *line) {
    if (!line)
        return 0;
    const char *p = skip_spaces(line);
    return strcmp(p, "نهاية_الشرط") == 0;
}

/* ================================================================ */
/* البحث عن عامل المقارنة مع مراعاة الأقواس                          */
/* ================================================================ */

static const char *find_condition_operator(const char *expr, int *op_len) {
    int depth = 0;
    const char *p = expr;

    while (*p) {
        if (*p == '(') {
            depth++;
        } else if (*p == ')') {
            if (depth > 0) depth--;
        } else if (depth == 0) {
            // العوامل المكونة من حرفين
            if (strncmp(p, ">=", 2) == 0) { *op_len = 2; return p; }
            if (strncmp(p, "<=", 2) == 0) { *op_len = 2; return p; }
            if (strncmp(p, "=!", 2) == 0) { *op_len = 2; return p; }
            if (strncmp(p, "!=", 2) == 0) { *op_len = 2; return p; }
            if (strncmp(p, "==", 2) == 0) { *op_len = 2; return p; }
            if (strncmp(p, ">!", 2) == 0) { *op_len = 2; return p; } // مرادف لـ >
            if (strncmp(p, "<!", 2) == 0) { *op_len = 2; return p; } // مرادف لـ <
            // العوامل المكونة من حرف واحد
            if (*p == '=') { *op_len = 1; return p; }
            if (*p == '>') { *op_len = 1; return p; }
            if (*p == '<') { *op_len = 1; return p; }
        }
        p++;
    }
    return NULL;
}

/* ================================================================ */
/* تقييم الشرط                                                       */
/* ================================================================ */

int condition_evaluate(const char *condition) {
    if (!condition)
        return -1;

    const char *start = skip_spaces(condition);

    if (!starts_with(start, "اذا("))
        return -1;

    const char *p = start + strlen("اذا(");
    p = skip_spaces(p);

    const char *close = find_matching_paren(start);
    if (!close)
        return -1;

    const char *after = skip_spaces(close + 1);
    if (*after != '\0')
        return -1;

    char expression[256];
    int length = close - p;
    if (length <= 0 || length >= 256)
        return -1;

    memcpy(expression, p, length);
    expression[length] = '\0';

    /* ============================================================ */
    /* لوحة المفاتيح                                                 */
    /* ============================================================ */
    const char *keyboard = skip_spaces(expression);
    if (starts_with(keyboard, "لوحة_المفاتيح(")) {
        return keyboard_condition(keyboard);
    }

    /* ============================================================ */
    /* الشروط العادية – دعم عوامل المقارنة                           */
    /* ============================================================ */
    int op_len = 0;
    const char *op_pos = find_condition_operator(expression, &op_len);
    if (!op_pos || op_len == 0)
        return -1;

    // استخراج الطرف الأيسر
    int left_len = op_pos - expression;
    char left[128];
    if (left_len <= 0 || left_len >= 128)
        return -1;
    memcpy(left, expression, left_len);
    left[left_len] = '\0';

    // استخراج الطرف الأيمن (بعد العامل)
    const char *right_start = op_pos + op_len;
    if (*right_start == '\0')
        return -1;
    // نسخ الطرف الأيمن
    char right[128];
    int right_len = strlen(right_start);
    if (right_len <= 0 || right_len >= 128)
        return -1;
    memcpy(right, right_start, right_len);
    right[right_len] = '\0';

    // حساب القيم
    int error_left = 0, error_right = 0;
    double left_value = math_calculate(left, &error_left);
    double right_value = math_calculate(right, &error_right);

    if (error_left || error_right)
        return -1;

    // تحديد العامل الفعلي (تجاهل ! في >! و <!)
    char op = op_pos[0];
    if (op_len == 2) {
        if (op_pos[1] == '!') {
            // >! أو <! نعتبرهما > و < على التوالي
            // نبقى على op كما هو
        } else if (op_pos[1] == '=') {
            // >= أو <=
            if (op == '>') return (left_value >= right_value) ? 1 : 0;
            if (op == '<') return (left_value <= right_value) ? 1 : 0;
        } else if (op == '=' && op_pos[1] == '!') {
            // =! أو !=
            return (left_value != right_value) ? 1 : 0;
        } else if (op == '=' && op_pos[1] == '=') {
            // ==
            return (left_value == right_value) ? 1 : 0;
        }
    }

    // العوامل المكونة من حرف واحد
    if (op == '=') {
        return (left_value == right_value) ? 1 : 0;
    } else if (op == '>') {
        return (left_value > right_value) ? 1 : 0;
    } else if (op == '<') {
        return (left_value < right_value) ? 1 : 0;
    }

    return -1;
}
