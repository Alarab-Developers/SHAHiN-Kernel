#include "التعليقات.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"

int comment_remove(const char *script, uint32_t size, char **cleaned, uint32_t *cleaned_size)
{
    if (!script || !cleaned || !cleaned_size)
        return -1;

    *cleaned = NULL;
    *cleaned_size = 0;

    // تخصيص مخزن مؤقت بنفس حجم النص (قد يكون أصغر بعد إزالة التعليقات)
    char *out = (char*)memory_api.alloc(size + 1);
    if (!out)
        return -1;

    uint32_t i = 0;   // مؤشر القراءة في النص الأصلي
    uint32_t j = 0;   // مؤشر الكتابة في النص الناتج
    int in_comment = 0;

    while (i < size)
    {
        if (!in_comment && i + 1 < size && script[i] == '"' && script[i + 1] == '"')
        {
            // بداية تعليق
            in_comment = 1;
            i += 2; // تخطي علامتي البداية
        }
        else if (in_comment && i + 1 < size && script[i] == '"' && script[i + 1] == '"')
        {
            // نهاية تعليق
            in_comment = 0;
            i += 2; // تخطي علامتي النهاية
        }
        else
        {
            // نسخ الحرف إذا لم نكن داخل تعليق
            if (!in_comment)
                out[j++] = script[i];
            i++;
        }
    }

    // إذا انتهى النص ونحن لا زلنا داخل تعليق => خطأ
    if (in_comment)
    {
        memory_api.free(out);
        return -1;
    }

    out[j] = '\0';
    *cleaned = out;
    *cleaned_size = j;
    return 0;
}
