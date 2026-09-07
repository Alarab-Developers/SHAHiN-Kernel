#include "المحلل.h"
#include "../لغة.h"
#include "../ادوات/ادوات.h"
#include "../المنفذ/المنفذ.h"
#include "مفاهيم_اللغة/الدوال.h"
#include "مفاهيم_اللغة/الحلقات.h"
#include "مفاهيم_اللغة/الشروط.h"
#include "مفاهيم_اللغة/رياضيات.h"
#include "مفاهيم_اللغة/المتغيرات.h"
#include "مفاهيم_اللغة/مكتبات_اللغة.h"
#include "مفاهيم_اللغة/الذاكرة_الخام.h"
#include "اوامر_اللغة/اوامر_اللغة.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "القلب/اختبار/مؤشر_الفأرة/مؤشر_الفأرة.h"

int arp_loop_depth = 0;
int arp_function_depth = 0;
double arp_return_value = 0;
int arp_return_type = ARP_RETURN_NONE;
char arp_return_text[256] = {0};


/* ================================================================ */
/* parse_function_arguments – ديناميكي (تخصيص المصفوفة مؤقتاً)     */
/* ================================================================ */
int parse_function_arguments(const char *p, double **arguments_out, int *argument_count) {
    if (!p || !arguments_out || !argument_count) return -1;
    *argument_count = 0;
    p = skip_spaces(p);
    if (*p == ')') {
        *arguments_out = NULL;
        *argument_count = 0;
        return 0;
    }
    double *args = NULL;
    int count = 0;
    while (*p) {
        double *new_args = (double*)memory_api.alloc((count + 1) * sizeof(double));
        if (!new_args) { if (args) memory_api.free(args); return -1; }
        if (args) {
            for (int i = 0; i < count; i++) new_args[i] = args[i];
            memory_api.free(args);
        }
        args = new_args;
        char expression[256];
        int length = 0;
        int depth = 0;
        while (*p && length < 255) {
            if (*p == '(') depth++;
            else if (*p == ')') { if (depth == 0) break; depth--; }
            if (*p == ',' && depth == 0) break;
            expression[length++] = *p++;
        }
        while (length > 0 && (expression[length-1] == ' ' || expression[length-1] == '\t' ||
                              expression[length-1] == '\r' || expression[length-1] == '\n')) length--;
        expression[length] = '\0';
        if (length == 0) { if (args) memory_api.free(args); return -1; }
        int error = 0;
        double value = math_calculate(expression, &error);
        if (error) { if (args) memory_api.free(args); return -1; }
        args[count] = value;
        count++;
        p = skip_spaces(p);
        if (*p == ')') {
            *arguments_out = args;
            *argument_count = count;
            return 0;
        }
        if (*p != ',') { if (args) memory_api.free(args); return -1; }
        p++;
        p = skip_spaces(p);
        if (*p == ')') { if (args) memory_api.free(args); return -1; }
    }
    if (args) memory_api.free(args);
    return -1;
}

/* ================================================================ */
/* parse_numeric_argument – لم يتغير (يستخدم مخزن مؤقت ثابت)      */
/* ================================================================ */
int parse_numeric_argument(const char **p, double *value, int last) {
    if (!p || !*p || !value) return -1;
    *p = skip_spaces(*p);
    char expression[256];
    int length = 0;
    int depth = 0;
    while (**p && length < 255) {
        if (**p == '(') depth++;
        else if (**p == ')') { if (depth == 0) break; depth--; }
        if (!last && **p == ',' && depth == 0) break;
        expression[length++] = **p;
        (*p)++;
    }
    while (length > 0 && (expression[length-1] == ' ' || expression[length-1] == '\t' ||
                          expression[length-1] == '\r' || expression[length-1] == '\n')) length--;
    expression[length] = '\0';
    if (length == 0) return -1;
    int error = 0;
    *value = math_calculate(expression, &error);
    if (error) return -1;
    *p = skip_spaces(*p);
    if (!last) {
        if (**p != ',') return -1;
        (*p)++;
    }
    return 0;
}

/* ================================================================ */
/* استدعاء دالة مسجّلة باسمها (مشترك بين كل السياقات: إسناد،        */
/* ارجع()، اطبع()، والتعبيرات الحسابية)                            */
/* ================================================================ */
int arp_invoke_function(const char *name, const char *args_start)
{
    if (!name || !args_start) return -1;

    arp_function_t *f = function_find(name);
    if (!f) return -1;

    double *arguments = NULL;
    int argument_count = 0;

    if (parse_function_arguments(args_start, &arguments, &argument_count) < 0) {
        if (arguments) memory_api.free(arguments);
        return -1;
    }

    if (argument_count != f->parameter_count) {
        if (arguments) memory_api.free(arguments);
        return -1;
    }

    for (int i = 0; i < f->parameter_count; i++) {
        if (variable_set(f->parameters[i], arguments[i]) < 0) {
            if (arguments) memory_api.free(arguments);
            return -1;
        }
    }
    if (arguments) memory_api.free(arguments);

    arp_return_type = ARP_RETURN_NONE;

    arp_function_depth++;
    int res = arp_execute_lines((const char**)f->lines, f->line_count);
    arp_function_depth--;

    if (res == -4) return -4;   // ارجع() نُفِّذت، القيمة جاهزة في arp_return_*
    if (res == -2) return -2;
    if (res == -3) return -3;
    if (res < 0) return -1;
    return 0;
}

/* ================================================================ */
/* تنفيذ مجموعة أسطر (تم تعديله لمعالجة -2)                        */
/* ================================================================ */
int arp_execute_lines(const char **lines, int line_count) {
    if (!lines || line_count < 0) return -1;
    int i = 0;
    while (i < line_count) {
        const char *line = lines[i];
        const char *p = skip_spaces(line);
        if (*p == '\0') { i++; continue; }
        if (loop_is_start(p)) {
            int loop_end = -1;
            int depth = 1;
            for (int j = i + 1; j < line_count; j++) {
                const char *b = skip_spaces(lines[j]);
                if (loop_is_start(b)) { depth++; continue; }
                if (loop_is_end(b)) { depth--; if (depth == 0) { loop_end = j; break; } }
            }
            if (loop_end < 0) return -1;
            int body_start = i + 1;
            int body_count = loop_end - body_start;
            int res = loop_execute(&lines[body_start], body_count);
            if (res == -4) return -4;
            if (res == -2) return -2;
            if (res < 0) return -1;
            i = loop_end + 1;
            continue;
        }
        if (raw_memory_is_start(p)) {
            int memory_end = -1;
            int depth = 1;
            for (int j = i + 1; j < line_count; j++) {
                const char *b = skip_spaces(lines[j]);
                if (raw_memory_is_start(b)) { depth++; continue; }
                if (raw_memory_is_end(b)) { depth--; if (depth == 0) { memory_end = j; break; } continue; }
            }
            if (memory_end < 0) return -1;
            int body_start = i + 1;
            int body_count = memory_end - body_start;
            if (body_count > 0) {
                int res = arp_execute_lines(&lines[body_start], body_count);
                if (res == -4) return -4;
                if (res == -2) return -2;
                if (res == -3) return -3;
                if (res < 0) return -1;
            }
            i = memory_end + 1;
            continue;
        }
        if (condition_is_start(p)) {
            int condition_start = i;
            int condition_end = -1;
            int else_line = -1;
            int depth = 1;
            for (int j = i + 1; j < line_count; j++) {
                const char *b = skip_spaces(lines[j]);
                if (condition_is_start(b)) { depth++; continue; }
                if (condition_is_end(b)) { depth--; if (depth == 0) { condition_end = j; break; } continue; }
                if (condition_is_else(b)) { if (depth == 1 && else_line < 0) else_line = j; }
            }
            if (condition_end < 0) return -1;
            if (loop_is_end(p) || condition_is_else(p)) return -1;
            int result = condition_evaluate(lines[condition_start]);
            if (result < 0) return -1;
            if (result == 1) {
                int body_start = condition_start + 1;
                int body_end = (else_line >= 0) ? else_line : condition_end;
                int body_count = body_end - body_start;
                if (body_count > 0) {
                    int res = arp_execute_lines(&lines[body_start], body_count);
                    if (res == -4) return -4;
                    if (res == -2) return -2;
                    if (res < 0) return -1;
                }
            } else {
                if (else_line >= 0) {
                    int body_start = else_line + 1;
                    int body_count = condition_end - body_start;
                    if (body_count > 0) {
                        int res = arp_execute_lines(&lines[body_start], body_count);
                        if (res == -4) return -4;
                        if (res == -2) return -2;
                        if (res < 0) return -1;
                    }
                }
            }
            i = condition_end + 1;
            continue;
        }
        if (raw_memory_is_end(p)) return -1;
        if (condition_is_end(p)) return -1;
        int res = arp_execute_line(line);
        if (res == -4) return -4;
        if (res == -2) return -2;
        if (res == -3) return -3;
        if (res < 0) return -1;
        i++;
    }
    return 0;
}

/* ================================================================ */
/* هل السطر بالكامل مجرد استدعاء دالة: اسم(معاملات) بدون أي شيء     */
/* قبله أو بعده؟ (نسخة محلية من نفس الفحص المستخدم في تحليل         */
/* الإسناد/الطباعة، تُستخدم هنا فقط لبوابة "ممنوع خارج الدوال")     */
/* ================================================================ */
static int line_is_bare_function_call(const char *p, const char **open_paren_out)
{
    if (!p || !*p) return 0;

    const char *open = find_char(p, '(');
    if (!open || open == p) return 0;

    for (const char *q = p; q < open; q++) {
        char c = *q;
        if (c == ' ' || c == '\t' || c == '+' || c == '-' || c == '*' ||
            c == '/' || c == ',' || c == '"' || c == '=' || c == '<' ||
            c == '>' || c == '!') {
            return 0;
        }
    }

    int depth = 0;
    const char *q = open;
    const char *close = NULL;
    while (*q) {
        if (*q == '(') depth++;
        else if (*q == ')') { depth--; if (depth == 0) { close = q; break; } }
        q++;
    }
    if (!close) return 0;

    const char *after = skip_spaces(close + 1);
    if (*after != '\0') return 0;

    *open_paren_out = open;
    return 1;
}

/* ================================================================ */
/* arp_execute_line – يستخدم المعاملات الديناميكية                */
/* ================================================================ */
int arp_execute_line(const char *line) {
    if (!line) return -1;
    const char *p = skip_spaces(line);
    if (*p == '\0') return 0;

    /* ============================================================ */
    /* ممنوع تنفيذ أي أمر خارج الدوال.                                */
    /*                                                                */
    /* الاستثناءان الوحيدان المسموحان على مستوى السكربت الرئيسي      */
    /* (أي عندما arp_function_depth == 0، أي لسنا داخل جسم أي دالة):  */
    /*                                                                */
    /*   - استدعاء مكتبة:      استدعاء("اسم.تطبيق")                  */
    /*   - استدعاء دالة معرّفة: اسم_الدالة(معاملات)   مثل: مرحبا()   */
    /*                                                                */
    /* (التعليقات لا تصل أصلاً إلى هذه الدالة؛ تُزال قبل التنفيذ في   */
    /* comment_remove) أي أمر آخر — اطبع، مستطيل، خلفية، شغل، عتاد،   */
    /* انتظر، ذاكرة_*، إسناد متغير (=)، استمر/اعد/كسر/ارجع، ... —    */
    /* يُرفض فوراً بخطأ ما دام خارج جسم دالة.                        */
    /* ============================================================ */
    if (arp_function_depth == 0) {
        if (starts_with(p, "استدعاء(")) {
            return parse_library_import(p);
        }

        const char *call_open;
        if (line_is_bare_function_call(p, &call_open)) {
            char name[64];
            int name_length = call_open - p;
            if (name_length <= 0 || name_length >= 64) return -1;
            memcpy(name, p, name_length);
            name[name_length] = '\0';
            int res = arp_invoke_function(name, call_open + 1);
            if (res == -4) return 0;   // نُفِّذ استدعاء كسطر مستقل؛ القيمة المُرجعة (إن وجدت) تُهمل
            if (res == -2) return -2;
            if (res == -3) return -3;
            if (res < 0) return -1;
            return 0;
        }

        return -1;
    }

    /* ============================================================ */
    /* ما يلي لا يُنفَّذ إلا داخل دالة (arp_function_depth > 0)       */
    /* ============================================================ */

    /* ============================================================ */
    /* أمر الاستمرار (continue)                                      */
    /* ============================================================ */
    if (starts_with(p, "استمر(")) {
        const char *q = skip_prefix(p, "استمر(");
        q = skip_spaces(q);
        if (*q != ')') return -1;
        q++;
        q = skip_spaces(q);
        if (*q != '\0') return -1;
        if (arp_loop_depth <= 0) return -1;
        return -2;
    }
    if (starts_with(p, "اعد(")) {
        const char *q = skip_prefix(p, "اعد(");
        q = skip_spaces(q);
        if (*q != ')') return -1;
        q++;
        q = skip_spaces(q);
        if (*q != '\0') return -1;
        if (arp_loop_depth <= 0) return -1;
        return -2;
    }

    /* ============================================================ */
    /* أمر كسر الحلقة (break)                                       */
    /* ============================================================ */
    if (starts_with(p, "كسر(")) {
        const char *q = skip_prefix(p, "كسر(");
        q = skip_spaces(q);
        if (*q != ')') return -1;
        q++;
        q = skip_spaces(q);
        if (*q != '\0') return -1;
        if (arp_loop_depth <= 0) return -1;
        return -3;
    }

    /* ============================================================ */
    /* أمر ارجع (return)                                            */
    /* ============================================================ */
    if (starts_with(p, "ارجع(")) {
        if (parse_return(p) < 0) return -1;
        return -4;   // إشارة إرجاع
    }

    variable_set("حركة_س", mouse_dx);
    variable_set("حركة_ص", mouse_dy);

    if (starts_with(p, "خلفية(")) return parse_background(p);
    if (starts_with(p, "احسب")) return parse_calculate(p);
    if (starts_with(p, "شغل(")) return parse_app(p);
    if (starts_with(p, "استدعاء(")) return parse_library_import(p);
    if (starts_with(p, "مستطيل(")) return parse_rect(p);
    if (starts_with(p, "مثلث(")) return parse_triangle(p);
    if (starts_with(p, "اطبع(")) return parse_print(p);
    if (starts_with(p, "ادخل(")) return parse_input(p);
    if (starts_with(p, "اعرض(")) return parse_present(p);
    if (starts_with(p, "عتاد(")) return parse_hardware(p);
    if (starts_with(p, "اقرا_عتاد(")) return parse_read_hardware(p);
    if (starts_with(p, "انتظر(")) return parse_wait(p);
    if (starts_with(p, "ذاكرة_اكتب(")) return parse_memory_write(p);
    if (starts_with(p, "ذاكرة_اقرا(")) return parse_memory_read(p);
    if (starts_with(p, "ذاكرة_")) {
        int matched = 0;
        int res = raw_memory_dispatch_statement(p, &matched);
        if (matched) return res;
    }
    if (find_char(p, '=')) return parse_assignment(p);

    const char *open = find_char(p, '(');
    if (open) {
        char name[64];
        int name_length = open - p;
        while (name_length > 0 && (p[name_length-1] == ' ' || p[name_length-1] == '\t')) name_length--;
        if (name_length <= 0 || name_length >= 64) return -1;
        memcpy(name, p, name_length);
        name[name_length] = '\0';
        int res = arp_invoke_function(name, open + 1);
        if (res == -4) return 0;   // نُفِّذ استدعاء كسطر مستقل؛ القيمة المُرجعة (إن وجدت) تُهمل
        if (res == -2) return -2;
        if (res == -3) return -3;
        if (res < 0) return -1;
        return 0;
    }
    return -1;
}
