#include "ادوات.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"

/* ================================================================ */
/* قراءة سطر – تخصيص ديناميكي                                       */
/* ================================================================ */
int read_line(
    const char *script,
    uint32_t size,
    uint32_t *offset,
    char **line_ptr,
    uint32_t *line_len
)
{
    if (!script || !offset || !line_ptr || !line_len)
        return -1;

    uint32_t start = *offset;
    uint32_t len = 0;

    while (*offset < size && script[*offset] != '\n' && script[*offset] != '\r')
    {
        (*offset)++;
        len++;
    }

    if (len == 0)
    {
        *line_ptr = NULL;
        *line_len = 0;
        if (*offset < size && script[*offset] == '\r') (*offset)++;
        if (*offset < size && script[*offset] == '\n') (*offset)++;
        return 0;
    }

    char *line = (char*)memory_api.alloc(len + 1);
    if (!line) return -1;

    for (uint32_t i = 0; i < len; i++)
        line[i] = script[start + i];
    line[len] = '\0';

    *line_ptr = line;
    *line_len = len;

    if (*offset < size && script[*offset] == '\r') (*offset)++;
    if (*offset < size && script[*offset] == '\n') (*offset)++;

    return 0;
}


const char *skip_spaces(const char *p)
{
    if (!p) return p;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

int starts_with(const char *text, const char *prefix)
{
    if (!text || !prefix) return 0;
    while (*prefix)
    {
        if (*text != *prefix) return 0;
        text++; prefix++;
    }
    return 1;
}

const char *skip_prefix(const char *p, const char *prefix)
{
    if (!p || !prefix) return p;
    while (*prefix) { p++; prefix++; }
    return p;
}

const char *find_char(const char *text, char target)
{
    if (!text) return 0;
    while (*text)
    {
        if (*text == target) return text;
        text++;
    }
    return 0;
}

void double_to_string(double value, char *buffer, int precision)
{
    if (!buffer) return;
    int pos = 0;
    if (value < 0) { buffer[pos++] = '-'; value = -value; }
    uint64_t integer = (uint64_t)value;
    char temp[32];
    int count = 0;
    if (integer == 0) temp[count++] = '0';
    else
    {
        while (integer > 0)
        {
            temp[count++] = '0' + (integer % 10);
            integer /= 10;
        }
    }
    while (count > 0) buffer[pos++] = temp[--count];
    double fraction = value - (uint64_t)value;
    if (fraction > 0 && precision > 0)
    {
        buffer[pos++] = '.';
        for (int i = 0; i < precision; i++)
        {
            fraction *= 10;
            int digit = (int)fraction;
            buffer[pos++] = '0' + digit;
            fraction -= digit;
            if (fraction == 0) break;
        }
    }
    buffer[pos] = '\0';
}

double string_to_double(const char *text, const char **end)
{
    if (!text) { if (end) *end = text; return 0; }
    const char *p = skip_spaces(text);
    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    else if (*p == '+') p++;
    double result = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9')
    {
        result = result * 10 + (*p - '0');
        p++;
        digits++;
    }
    if (*p == '.')
    {
        p++;
        double divisor = 10.0;
        while (*p >= '0' && *p <= '9')
        {
            result += (*p - '0') / divisor;
            divisor *= 10.0;
            p++;
            digits++;
        }
    }
    if (digits == 0) { if (end) *end = text; return 0; }
    if (end) *end = p;
    return result * sign;
}

char *arp_strdup(const char *source)
{
    if (!source) return NULL;
    size_t len = strlen(source) + 1;
    char *copy = (char*)memory_api.alloc(len);
    if (!copy) return NULL;
    memcpy(copy, source, len);
    return copy;
}

char *arp_strndup(const char *source, int length)
{
    if (!source || length < 0) return NULL;
    char *copy = (char*)memory_api.alloc((size_t)length + 1);
    if (!copy) return NULL;
    memcpy(copy, source, (size_t)length);
    copy[length] = '\0';
    return copy;
}

/* ================================================================ */
/* تطبيع مسار للتضمين/الاستيراد إلى جذر الأرشيف                     */
/* ================================================================ */
void arp_normalize_archive_path(const char *src, char *dst, int dst_size)
{
    if (!dst || dst_size <= 0) { if (dst) dst[0] = '\0'; return; }
    if (!src) { dst[0] = '\0'; return; }

    int i = 0, j = 0;

    /* تجاهل أي بادئة "./" أو ".\" متكررة */
    while (src[i] == '.' && (src[i+1] == '\\' || src[i+1] == '/'))
        i += 2;

    /* تجاهل أي شرطات بادئة */
    while (src[i] == '/' || src[i] == '\\')
        i++;

    int last_was_slash = 0;
    while (src[i] && j < dst_size - 1)
    {
        char c = src[i];
        if (c == '\\') c = '/';

        if (c == '/')
        {
            /* لا نبدأ بشرطة، ولا نكرّرها */
            if (j == 0 || last_was_slash) { i++; continue; }
            last_was_slash = 1;
        }
        else
        {
            last_was_slash = 0;
        }

        dst[j++] = c;
        i++;
    }
    dst[j] = '\0';
}
