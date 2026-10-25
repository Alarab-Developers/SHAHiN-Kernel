#include "التعليقات.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"

/* =========================================================
 * رموز التعليقات (UTF-8):
 *   ؛    = U+061B  →  [0xD8, 0x9B]              (تعليق فردي)
 *   ؛*   = U+061B ثم '*'  →  [0xD8, 0x9B, '*']  (بداية جماعي)
 *   *؛   = '*' ثم U+061B  →  ['*', 0xD8, 0x9B]  (نهاية جماعي)
 * ========================================================= */

#define ARP_SEMI_LEN   2
#define ARP_OPEN_LEN   3
#define ARP_CLOSE_LEN  3

static int is_semi(const char *p, uint32_t remaining)
{
    return remaining >= ARP_SEMI_LEN &&
           (unsigned char)p[0] == 0xD8 &&
           (unsigned char)p[1] == 0x9B;
}

static int is_open(const char *p, uint32_t remaining)
{
    return remaining >= ARP_OPEN_LEN &&
           (unsigned char)p[0] == 0xD8 &&
           (unsigned char)p[1] == 0x9B &&
           p[2] == '*';
}

static int is_close(const char *p, uint32_t remaining)
{
    return remaining >= ARP_CLOSE_LEN &&
           p[0] == '*' &&
           (unsigned char)p[1] == 0xD8 &&
           (unsigned char)p[2] == 0x9B;
}

/* =========================================================
 * comment_remove
 *
 * إزالة التعليقات من السكربت.
 *
 * القواعد:
 *   - ؛         → تعليق فردي حتى نهاية السطر
 *   - ؛* ... *؛ → تعليق جماعي (يدعم التعشيش)
 *
 * النصوص بين " " لا تُعالج (محفوظة كما هي).
 * داخل التعليق الجماعي، النصوص " " تُتجاهل تمامًا.
 * عند مواجهة تعليق جماعي غير مغلق → خطأ (-1).
 * ========================================================= */
int comment_remove(const char *script, uint32_t size,
                   char **cleaned, uint32_t *cleaned_size)
{
    if (!script || !cleaned || !cleaned_size)
        return -1;

    *cleaned = NULL;
    *cleaned_size = 0;

    char *out = (char *)memory_api.alloc(size + 1);
    if (!out)
        return -1;

    uint32_t i = 0;
    uint32_t j = 0;
    int depth = 0;        /* عمق التعشيش للتعليق الجماعي */
    int in_string = 0;    /* داخل نص "..." (خارج التعليقات فقط) */

    while (i < size)
    {
        /* ============================================
         * 1) داخل التعليق الجماعي: نبحث فقط عن
         *    ؛* (تعشيش) و *؛ (إغلاق)
         * ============================================ */
        if (depth > 0)
        {
            if (is_close(script + i, size - i))
            {
                depth--;
                i += ARP_CLOSE_LEN;
                continue;
            }
            if (is_open(script + i, size - i))
            {
                depth++;
                i += ARP_OPEN_LEN;
                continue;
            }
            i++;
            continue;
        }

        /* ============================================
         * 2) خارج التعليقات: نعالج النصوص والتعليقات
         * ============================================ */

        /* ---- داخل نص: ننسخ كل شيء حتى ينتهي النص ---- */
        if (in_string)
        {
            if (script[i] == '"')
                in_string = 0;
            out[j++] = script[i++];
            continue;
        }

        /* ---- بداية نص ---- */
        if (script[i] == '"')
        {
            in_string = 1;
            out[j++] = script[i++];
            continue;
        }

        /* ---- بداية تعليق جماعي: ؛* ---- */
        if (is_open(script + i, size - i))
        {
            depth = 1;
            i += ARP_OPEN_LEN;
            continue;
        }

        /* ---- تعليق فردي: ؛ ---- */
        if (is_semi(script + i, size - i))
        {
            i += ARP_SEMI_LEN;
            while (i < size && script[i] != '\n')
                i++;
            /* نحتفظ بالسطر الجديد */
            if (i < size && script[i] == '\n')
                out[j++] = script[i++];
            continue;
        }

        /* ---- محرف عادي ---- */
        out[j++] = script[i++];
    }

    /* تعليق جماعي غير مغلق */
    if (depth > 0)
    {
        memory_api.free(out);
        return -1;
    }

    out[j] = '\0';
    *cleaned = out;
    *cleaned_size = j;
    return 0;
}
