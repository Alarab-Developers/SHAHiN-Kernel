#include "اوامر_اللغة.h"
#include "../المحلل.h"
#include "../../لغة.h"
#include "../../ادوات/ادوات.h"
#include "../../المنفذ/المنفذ.h"
#include "../مفاهيم_اللغة/رياضيات.h"
#include "../مفاهيم_اللغة/المتغيرات.h"
#include "../مفاهيم_اللغة/مكتبات_اللغة.h"
#include "../مفاهيم_اللغة/الذاكرة_الخام.h"

static int parse_memory_read_value(const char *p, double *result);

/* ================================================================ */
/* هل p بالكامل مجرد اسم (متغير) بدون أي عوامل/أقواس/فواصل؟         */
/* ================================================================ */
static int is_bare_identifier(const char *p)
{
    if (!p || !*p) return 0;
    while (*p) {
        char c = *p;
        if (c == ' ' || c == '\t' || c == '(' || c == ')' || c == '+' ||
            c == '-' || c == '*' || c == '/' || c == ',' || c == '"' ||
            c == '=' || c == '<' || c == '>' || c == '!') {
            return 0;
        }
        p++;
    }
    return 1;
}

/* ================================================================ */
/* هل p بالكامل مجرد استدعاء دالة: اسم(معاملات) بدون أي شيء بعده؟   */
/* عند النجاح يضع open_paren_out على مكان القوس المفتوح             */
/* ================================================================ */
static int is_bare_function_call(const char *p, const char **open_paren_out)
{
    if (!p || !*p) return 0;

    const char *open = find_char(p, '(');
    if (!open || open == p) return 0;

    for (const char *q = p; q < open; q++) {
        char c = *q;
        if (c == ' ' || c == '\t' || c == '+' || c == '-' || c == '*' ||
            c == '/' || c == ',' || c == '"' || c == '=' || c == '<' ||
            c == '>' || c == '!' || c == '(') {
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
/* تحليل واكتشاف التمغيرات                                         */
/* ================================================================ */
int parse_assignment(const char *p){
    char name[64];
    int length = 0;
    p = skip_spaces(p);
    while (*p && *p != ' ' && *p != '\t' && *p != '=' && length < 63) {
        name[length++] = *p++;
    }
    name[length] = '\0';
    if (length == 0) return -1;
    p = skip_spaces(p);
    if (*p != '=') return -1;
    p++;
    p = skip_spaces(p);
    if (*p == '\0') return -1;
    if (starts_with(p, "ذاكرة_اقرا(")) {
        double value;
        if (parse_memory_read_value(p, &value) < 0) return -1;
        return variable_set(name, value);
    }
    {
        int matched = 0;
        double value;
        int res = raw_memory_dispatch_value(p, &value, &matched);
        if (matched) {
            if (res < 0) return -1;
            return variable_set(name, value);
        }
    }
    if (*p == '"') {
        p++;
        char text[256];
        int length = 0;
        while (*p && *p != '"' && length < 255) {
            text[length++] = *p++;
        }
        text[length] = '\0';
        if (*p != '"') return -1;
        p++;
        p = skip_spaces(p);
        if (*p != '\0') return -1;
        return variable_set_text(name, text);
    }

    /* اسم = دالة(معاملات) */
    const char *call_open;
    if (is_bare_function_call(p, &call_open)) {
        char func_name[64];
        int fn_len = call_open - p;
        if (fn_len <= 0 || fn_len >= 64) return -1;
        memcpy(func_name, p, fn_len);
        func_name[fn_len] = '\0';

        int result = arp_invoke_function(func_name, call_open + 1);
        if (result == -4) {
            if (arp_return_type == ARP_RETURN_TEXT) return variable_set_text(name, arp_return_text);
            if (arp_return_type == ARP_RETURN_NUMBER) return variable_set(name, arp_return_value);
            return -1; /* ارجع() بدون قيمة: لا يوجد ما يُسنَد */
        }
        return -1; /* خطأ أثناء التنفيذ، أو الدالة انتهت بدون ارجع() */
    }

    /* اسم = متغير_اخر (نسخ متغير نصي كما هو) */
    if (is_bare_identifier(p)) {
        char text_value[256];
        if (variable_get_text(p, text_value, sizeof(text_value)) == 0) {
            return variable_set_text(name, text_value);
        }
        /* ليس نصياً؛ نكمل بالمسار الرقمي أدناه */
    }

    int error = 0;
    double value = math_calculate(p, &error);
    if (error) return -1;
    return variable_set(name, value);
}

static const char *find_matching_paren(const char *p) {
    int depth = 0;
    while (*p) {
        if (*p == '(') depth++;
        else if (*p == ')') {
            if (depth == 0) return p;
            depth--;
        }
        p++;
    }
    return NULL;
}

int parse_return(const char *p)
{
    if (!starts_with(p, "ارجع("))
        return -1;

    const char *open =
        p + strlen("ارجع(");

    const char *close =
        find_matching_paren(open);

    if (!close)
        return -1;

    /* لا شيء مسموح بعد القوس المغلق */
    const char *after = skip_spaces(close + 1);
    if (*after != '\0')
        return -1;

    int expr_len =
        close - open;

    /*
     * ارجع()
     *
     * خروج من الدالة بدون قيمة
     */
    if (expr_len == 0)
    {
        arp_return_type = ARP_RETURN_NONE;
        return 0;
    }

    if (expr_len >= 256)
        return -1;

    char expr[256];

    memcpy(
        expr,
        open,
        expr_len
    );

    expr[expr_len] =
        '\0';

    /* تقليم المسافات من الطرفين */
    int start_i = 0, end_i = expr_len;
    while (start_i < end_i && (expr[start_i] == ' ' || expr[start_i] == '\t')) start_i++;
    while (end_i > start_i && (expr[end_i-1] == ' ' || expr[end_i-1] == '\t')) end_i--;

    char trimmed[256];
    int trimmed_len = end_i - start_i;
    memcpy(trimmed, expr + start_i, trimmed_len);
    trimmed[trimmed_len] = '\0';

    if (trimmed_len == 0)
    {
        arp_return_type = ARP_RETURN_NONE;
        return 0;
    }

    /*
     * ارجع("نص")
     */
    if (trimmed[0] == '"')
    {
        if (trimmed_len < 2 || trimmed[trimmed_len-1] != '"')
            return -1;

        int text_len = trimmed_len - 2;
        if (text_len < 0 || text_len >= (int)sizeof(arp_return_text))
            return -1;

        memcpy(arp_return_text, trimmed + 1, text_len);
        arp_return_text[text_len] = '\0';
        arp_return_type = ARP_RETURN_TEXT;
        return 0;
    }

    /*
     * ارجع(دالة_اخرى(معاملات))
     */
    const char *call_open;
    if (is_bare_function_call(trimmed, &call_open))
    {
        char func_name[64];
        int fn_len = call_open - trimmed;
        if (fn_len <= 0 || fn_len >= 64) return -1;
        memcpy(func_name, trimmed, fn_len);
        func_name[fn_len] = '\0';

        int result = arp_invoke_function(func_name, call_open + 1);
        if (result == -4) return 0; /* arp_return_* موضوعة مسبقاً من الاستدعاء الداخلي */
        return -1;
    }

    /*
     * ارجع(متغير_نصي)
     */
    char text_value[256];
    if (variable_get_text(trimmed, text_value, sizeof(text_value)) == 0)
    {
        int len = strlen(text_value);
        memcpy(arp_return_text, text_value, len + 1);
        arp_return_type = ARP_RETURN_TEXT;
        return 0;
    }

    /*
     * ارجع(تعبير رقمي)
     */
    int error = 0;

    double value =
        math_calculate(
            trimmed,
            &error
        );

    if (error)
        return -1;

    arp_return_value =
        value;
    arp_return_type =
        ARP_RETURN_NUMBER;

    return 0;
}

/* ================================================================ */
/* تحليل استدعاء مكتبة                                               */
/* ================================================================ */
int parse_library_import(const char *p) {
    p = skip_prefix(p, "استدعاء(");
    p = skip_spaces(p);
    char name[128];
    int length = 0;
    while (*p && *p != ')' && *p != ' ' && *p != '\t' && length < 127) {
        name[length++] = *p++;
    }
    name[length] = '\0';
    p = skip_spaces(p);
    if (*p != ')') return -1;
    if (length == 0) return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    return library_import(name);
}

/* ================================================================ */
/* تحليل أمر احسب                                                    */
/* ================================================================ */
int parse_calculate(const char *p) {
    p = skip_prefix(p, "احسب");
    p = skip_spaces(p);
    if (*p != '=') return -1;
    p++;
    p = skip_spaces(p);
    if (*p == '\0') return -1;
    int error = 0;
    double result = math_calculate(p, &error);
    if (error) return -1;
    return execute_calculate(result);
}

/* ================================================================ */
/* تحليل أمر شغل                                                     */
/* ================================================================ */
int parse_app(const char *p) {
    p = skip_prefix(p, "شغل(");
    p = skip_spaces(p);
    char name[256];
    int length = 0;
    while (*p && *p != ')' && length < 255) {
        name[length++] = *p++;
    }
    name[length] = '\0';
    while (length > 0 && (name[length - 1] == ' ' || name[length - 1] == '\t')) {
        name[--length] = '\0';
    }
    if (length == 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    return execute_app(name);
}

/* ================================================================ */
/* تحليل أمر مستطيل                                                  */
/* ================================================================ */
int parse_rect(const char *p) {
    if (!p) return -1;
    p = skip_prefix(p, "مستطيل(");
    double x, y, width, height, color;
    if (parse_numeric_argument(&p, &x, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &y, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &width, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &height, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &color, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    return execute_rect((int)x, (int)y, (int)width, (int)height, (uint32_t)color);
}

/* ================================================================ */
/* تحليل أمر خلفية                                                   */
/* ================================================================ */
int parse_background(const char *p) {
    p = skip_prefix(p, "خلفية(");
    p = skip_spaces(p);
    if (*p == '"') {
        p++;
        char path[256];
        int length = 0;
        while (*p && *p != '"' && length < 255) {
            path[length++] = *p++;
        }
        path[length] = '\0';
        if (*p != '"') return -1;
        p++;
        p = skip_spaces(p);
        if (*p != ')') return -1;
        p++;
        p = skip_spaces(p);
        if (*p != '\0') return -1;
        if (length == 0) return -1;
    }
    uint32_t color = 0;
    if (p[0] != '0' || (p[1] != 'x' && p[1] != 'X')) return -1;
    p += 2;
    int digits = 0;
    while (*p) {
        char c = *p;
        uint32_t value;
        if (c >= '0' && c <= '9') value = (uint32_t)(c - '0');
        else if (c >= 'A' && c <= 'F') value = (uint32_t)(c - 'A' + 10);
        else if (c >= 'a' && c <= 'f') value = (uint32_t)(c - 'a' + 10);
        else break;
        color = (color << 4) | value;
        digits++;
        p++;
    }
    if (digits == 0) return -1;
    p = skip_spaces(p);
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    return 0;
}

//======================= العتاد
int parse_hardware(const char *p) {
    p = skip_prefix(p, "عتاد(");
    p = skip_spaces(p);
    double port, value;
    if (parse_numeric_argument(&p, &port, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &value, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    return execute_hardware((uint16_t)port, (uint8_t)value);
}

int parse_read_hardware(const char *p) {
    p = skip_prefix(p, "اقرا_عتاد(");
    p = skip_spaces(p);
    double port;
    if (parse_numeric_argument(&p, &port, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    execute_read_hardware((uint16_t)port);
    return 0;
}

/* ================================================================ */
/* تحليل أمر انتظر                                                   */
/* ================================================================ */
int parse_wait(const char *p) {
    if (!p) return -1;
    p = skip_prefix(p, "انتظر(");
    p = skip_spaces(p);
    double count;
    if (parse_numeric_argument(&p, &count, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    if (count < 0) return -1;
    return execute_wait((uint64_t)count);
}

/* ================================================================ */
/* تحليل أمر مثلث                                                    */
/* ================================================================ */
int parse_triangle(const char *p) {
    if (!p) return -1;
    p = skip_prefix(p, "مثلث(");
    p = skip_spaces(p);
    double x1, y1, x2, y2, x3, y3, color;
    if (parse_numeric_argument(&p, &x1, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &y1, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &x2, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &y2, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &x3, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &y3, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &color, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    return execute_triangle((int)x1, (int)y1, (int)x2, (int)y2, (int)x3, (int)y3, (uint32_t)color);
}

int parse_present(const char *p) {
    p += strlen("اعرض");
    while (*p == ' ') p++;
    if (*p != '(') return -1;
    p++;
    while (*p == ' ') p++;
    if (*p != ')') return -1;
    return execute_present();
}

/* ================================================================ */
/* تحليل أمر اطبع                                                    */
/* ================================================================ */
int parse_print(const char *p) {
    if (!p) return -1;
    p = skip_prefix(p, "اطبع(");
    p = skip_spaces(p);
    char output[512] = {0};
    int output_pos = 0;
    if (*p == ')') {
        p++;
        p = skip_spaces(p);
        if (*p != '\0') return -1;
        return execute_print(output);
    }
    while (*p && *p != ')') {
        p = skip_spaces(p);
        if (*p == '"') {
            p++;
            while (*p && *p != '"' && output_pos < 511) {
                output[output_pos++] = *p;
                p++;
            }
            if (*p != '"') return -1;
            p++;
        } else {
            const char *start = p;
            int depth = 0;
            while (*p && !(*p == ',' && depth == 0) && !(*p == ')' && depth == 0)) {
                if (*p == '(') depth++;
                else if (*p == ')') depth--;
                p++;
            }
            int expr_len = p - start;
            if (expr_len <= 0 || expr_len >= 256) return -1;
            char expr[256];
            memcpy(expr, start, expr_len);
            expr[expr_len] = '\0';
            char text_value[256];
            if (variable_get_text(expr, text_value, sizeof(text_value)) == 0) {
                int len = strlen(text_value);
                if (output_pos + len >= (int)sizeof(output)) return -1;
                strcpy(output + output_pos, text_value);
                output_pos += len;
                p = skip_spaces(p);
                if (*p == ',') {
                    p++;
                    continue;
                }
                if (*p == ')') break;
                return -1;
            }

            /* متغير رقمي بسيط: نطبعه حسب نوعه الفعلي (صحيح/عائم)      */
            /* بدلاً من تمريره عبر محرك الرياضيات ثم تخمين النوع من     */
            /* وجود جزء عشري أو عدم وجوده                              */
            number_kind_t kind;
            if (is_bare_identifier(expr) && variable_get_kind(expr, &kind) == 0) {
                char num_str[64];
                if (variable_to_string(expr, num_str, sizeof(num_str)) < 0) return -1;
                int len = strlen(num_str);
                if (output_pos + len >= (int)sizeof(output)) return -1;
                strcpy(output + output_pos, num_str);
                output_pos += len;
                p = skip_spaces(p);
                if (*p == ',') {
                    p++;
                    continue;
                }
                if (*p == ')') break;
                return -1;
            }

            const char *call_open;
            if (is_bare_function_call(expr, &call_open)) {
                char func_name[64];
                int fn_len = call_open - expr;
                if (fn_len <= 0 || fn_len >= 64) return -1;
                memcpy(func_name, expr, fn_len);
                func_name[fn_len] = '\0';

                int result = arp_invoke_function(func_name, call_open + 1);
                if (result == -4 && arp_return_type == ARP_RETURN_TEXT) {
                    int len = strlen(arp_return_text);
                    if (output_pos + len >= (int)sizeof(output)) return -1;
                    strcpy(output + output_pos, arp_return_text);
                    output_pos += len;
                } else if (result == -4 && arp_return_type == ARP_RETURN_NUMBER) {
                    char num_str[64];
                    double_to_string(arp_return_value, num_str, 6);
                    int len = strlen(num_str);
                    if (output_pos + len >= (int)sizeof(output)) return -1;
                    strcpy(output + output_pos, num_str);
                    output_pos += len;
                } else {
                    return -1; /* خطأ، أو ارجع() بدون قيمة، أو دالة بدون ارجع */
                }

                p = skip_spaces(p);
                if (*p == ',') {
                    p++;
                    continue;
                }
                if (*p == ')') break;
                return -1;
            }

            int error = 0;
            double value = math_calculate(expr, &error);
            if (error) return -1;
            char num_str[64];
            double_to_string(value, num_str, 6);
            int len = strlen(num_str);
            if (output_pos + len < 512) {
                strcpy(output + output_pos, num_str);
                output_pos += len;
            } else return -1;
        }
        p = skip_spaces(p);
        if (*p == ',') {
            p++;
            continue;
        } else if (*p == ')') {
            break;
        } else return -1;
    }
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    return execute_print(output);
}

/* ================================================================ */
/* تحليل أمر ادخل                                                    */
/* ================================================================ */
int parse_input(const char *p) {
    if (!p) return -1;
    p = skip_prefix(p, "ادخل(");
    p = skip_spaces(p);
    char var_name[64];
    int len = 0;
    while (*p && *p != ')' && len < 63) var_name[len++] = *p++;
    var_name[len] = '\0';
    if (len == 0) return -1;
    p = skip_spaces(p);
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    return execute_input(var_name);
}

/* ================================================================ */
/* تحليل ذاكرة_اكتب                                                  */
/* ================================================================ */
int parse_memory_write(const char *p) {
    if (!p) return -1;
    p = skip_prefix(p, "ذاكرة_اكتب(");
    p = skip_spaces(p);
    double address, value;
    if (parse_numeric_argument(&p, &address, 0) < 0) return -1;
    if (parse_numeric_argument(&p, &value, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    return execute_memory_write((uint64_t)address, (uint32_t)value);
}

/* ================================================================ */
/* قراءة قيمة ذاكرة مع إرجاعها                                      */
/* ================================================================ */
static int parse_memory_read_value(const char *p, double *result) {
    if (!p || !result) return -1;
    p = skip_prefix(p, "ذاكرة_اقرا(");
    p = skip_spaces(p);
    double address;
    if (parse_numeric_argument(&p, &address, 1) < 0) return -1;
    if (*p != ')') return -1;
    p++;
    p = skip_spaces(p);
    if (*p != '\0') return -1;
    uint32_t value;
    if (execute_memory_read((uint64_t)address, &value) < 0) return -1;
    *result = (double)value;
    return 0;
}

int parse_memory_read(const char *p) {
    double value;
    return parse_memory_read_value(p, &value);
}
