#include "لوحة_المفاتيح.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مكتبة_واجهة_الإدخال_والإخراج.h"
#include "محرك_الجدولة/الأدارة/مُدير_قائمة_الأنتظار.h"

wait_queue_t keyboard_queue;

#define KB_BUFFER_SIZE 256

char kb_buffer[KB_BUFFER_SIZE];
int kb_head = 0;
int kb_tail = 0;
volatile char last_key = 0;

static int shift_pressed = 0;
static volatile unsigned char key_state[128] = {0};
static int extended_key = 0;
static int win_pressed  = 0;

/* اللغة الافتراضية: العربية */
static int keyboard_language = KB_LANG_AR;

int keyboard_get_language(void) { return keyboard_language; }
void keyboard_set_language(int lang) { keyboard_language = (lang == KB_LANG_EN) ? KB_LANG_EN : KB_LANG_AR; }

int keyboard_is_pressed(unsigned char key) {
    if (key >= 128) return 0;
    return key_state[key];
}

/* ===== الخرائط الإنجليزية ===== */
static const char keymap_en[128] = {
  0,27,'1','2','3','4','5','6','7','8',
 '9','0','-','=', '\b','\t',
 'q','w','e','r','t','y','u','i','o','p',
 '[',']','\n',0,
 'a','s','d','f','g','h','j','k','l',
 ';','\'','`',0,'\\',
 'z','x','c','v','b','n','m',
 ',','.','/',0,'*',0,' '
};

static const char keymap_en_shift[128] = {
  0,27,'!','@','#','$','%','^','&','*',
 '(',')','_','+', '\b','\t',
 'Q','W','E','R','T','Y','U','I','O','P',
 '{','}','\n',0,
 'A','S','D','F','G','H','J','K','L',
 ':','"','~',0,'|',
 'Z','X','C','V','B','N','M',
 '<','>','?',0,'*',0,' '
};

/* ===== الخرائط العربية (UTF-8) ===== */
/* كل حرف = سلسلة UTF-8. الأرقام تبقى لاتينية. */
static const char *keymap_ar[128] = {
    /* 0x00 */ 0,
    /* 0x01 Esc */ 0,
    /* 0x02 1 */ "١", "٢", "٣", "٤", "٥", "٦", "٧", "٨", "٩",
    /* 0x0B 0 */ "٠",
    /* 0x0C - */ "-",
    /* 0x0D = */ "=",
    /* 0x0E Backspace */ "\b",
    /* 0x0F Tab */ "\t",
    /* 0x10 Q */ "ض",
    /* 0x11 W */ "ص",
    /* 0x12 E */ "ث",
    /* 0x13 R */ "ق",
    /* 0x14 T */ "ف",
    /* 0x15 Y */ "غ",
    /* 0x16 U */ "ع",
    /* 0x17 I */ "ه",
    /* 0x18 O */ "خ",
    /* 0x19 P */ "ح",
    /* 0x1A [ */ "ج",
    /* 0x1B ] */ "د",
    /* 0x1C Enter */ "\n",
    /* 0x1D Ctrl */ 0,
    /* 0x1E A */ "ش",
    /* 0x1F S */ "س",
    /* 0x20 D */ "ي",
    /* 0x21 F */ "ب",
    /* 0x22 G */ "ل",
    /* 0x23 H */ "ا",
    /* 0x24 J */ "ت",
    /* 0x25 K */ "ن",
    /* 0x26 L */ "م",
    /* 0x27 ; */ "ك",
    /* 0x28 ' */ "ط",
    /* 0x29 ` */ "ذ",
    /* 0x2A LShift */ 0,
    /* 0x2B \ */ "\\",
    /* 0x2C Z */ "ئ",
    /* 0x2D X */ "ء",
    /* 0x2E C */ "ؤ",
    /* 0x2F V */ "ر",
    /* 0x30 B */ "لا",
    /* 0x31 N */ "ى",
    /* 0x32 M */ "ة",
    /* 0x33 , */ "و",
    /* 0x34 . */ "ز",
    /* 0x35 / */ "ظ",
    /* 0x36 RShift */ 0,
    /* 0x37 * */ "*",
    /* 0x38 LAlt */ 0,
    /* 0x39 Space */ " ",
    /* ما بعد 0x39 → NULL افتراضيًّا (init false) */
    /* نستخدم NULL كقيمة افتراضية تلقائيًّا للباقي */
};

static const char *keymap_ar_shift[128] = {
    /* 0x00 */ 0,
    /* 0x01 Esc */ 0,
    /* 0x02 1 */ "!", "@", "#", "$", "%", "^", "&", "*", "(",
    /* 0x0B 0 */ ")",
    /* 0x0C - */ "_",
    /* 0x0D = */ "+",
    /* 0x0E Backspace */ "\b",
    /* 0x0F Tab */ "\t",
    /* 0x10 Q */ "َ",   /* فتحة */
    /* 0x11 W */ "ً",   /* تنوين فتح */
    /* 0x12 E */ "ُ",   /* ضمة */
    /* 0x13 R */ "ٌ",   /* تنوين ضم */
    /* 0x14 T */ "لإ",
    /* 0x15 Y */ "إ",
    /* 0x16 U */ "'",
    /* 0x17 I */ "÷",
    /* 0x18 O */ "×",
    /* 0x19 P */ "؛",
    /* 0x1A [ */ "<",
    /* 0x1B ] */ ">",
    /* 0x1C Enter */ "\n",
    /* 0x1D Ctrl */ 0,
    /* 0x1E A */ "ِ",   /* كسرة */
    /* 0x1F S */ "ٍ",   /* تنوين كسر */
    /* 0x20 D */ "]",
    /* 0x21 F */ "[",
    /* 0x22 G */ "لأ",
    /* 0x23 H */ "أ",
    /* 0x24 J */ "ـ",   /* تطويل */
    /* 0x25 K */ "،",   /* فاصلة عربية */
    /* 0x26 L */ "/",
    /* 0x27 ; */ ":",
    /* 0x28 ' */ "\"",
    /* 0x29 ` */ "ّ",   /* شدة */
    /* 0x2A LShift */ 0,
    /* 0x2B \ */ "|",
    /* 0x2C Z */ "~",
    /* 0x2D X */ "ْ",   /* سكون */
    /* 0x2E C */ "}",
    /* 0x2F V */ "{",
    /* 0x30 B */ "لآ",
    /* 0x31 N */ "آ",
    /* 0x32 M */ "'",
    /* 0x33 , */ ",",
    /* 0x34 . */ ".",
    /* 0x35 / */ "؟",
    /* 0x36 RShift */ 0,
    /* 0x37 * */ "*",
    /* 0x38 LAlt */ 0,
    /* 0x39 Space */ " ",
    /* الباقي NULL افتراضيًّا */
};

/* الحصول على سلسلة UTF-8 المناسبة (NULL لو لا شيء) */
static const char *lookup_utf8(unsigned char code)
{
    if (code >= 128) return 0;

    const char *s = 0;

    if (keyboard_language == KB_LANG_AR)
        s = shift_pressed ? keymap_ar_shift[code] : keymap_ar[code];
    else
    {
        /* الإنجليزية: نحوّل char إلى نصّ من حرف واحد عبر مخزن ثابت */
        static char one[2];
        char c = shift_pressed ? keymap_en_shift[code] : keymap_en[code];
        if (c == 0) return 0;
        one[0] = c;
        one[1] = '\0';
        return one;
    }

    return s;
}

char keyboard_getchar() {
    /* نسخة بسيطة متوافقة مع الاستخدام القديم (تُرجع ASCII أوّل بايت فقط) */
    while (!(inb(0x64) & 1));
    unsigned char scancode = inb(0x60);
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return 0; }
    if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = 0; return 0; }
    if (scancode & 0x80) return 0;
    if (scancode >= 128) return 0;
    const char *s = lookup_utf8(scancode);
    return (s && s[0]) ? s[0] : 0;
}

void keyboard_init(void) {
    shift_pressed = 0;
    win_pressed   = 0;
    extended_key  = 0;
    kb_head = 0;
    kb_tail = 0;
    for (int i = 0; i < 128; i++)
        key_state[i] = 0;
    keyboard_language = KB_LANG_AR;   /* عربي افتراضياً */
    wait_queue_init(&keyboard_queue);
}

/* دفع سلسلة UTF-8 كاملة إلى المخزن */
static void kb_push_utf8(const char *s)
{
    if (!s || !*s) return;

    int pushed = 0;
    while (*s)
    {
        int next = (kb_head + 1) % KB_BUFFER_SIZE;
        if (next == kb_tail) break;   /* المخزن ممتلئ */
        kb_buffer[kb_head] = *s++;
        kb_head = next;
        pushed = 1;
    }
    if (pushed) {
        last_key = kb_buffer[(kb_head - 1 + KB_BUFFER_SIZE) % KB_BUFFER_SIZE];
        wake_up_one(&keyboard_queue);
    }
}

void keyboard_handler() {
    unsigned char scancode = inb(0x60);

    /* ===== Extended prefix ===== */
    if (scancode == 0xE0) {
        extended_key = 1;
        return;
    }

    if (extended_key) {
        int release = scancode & 0x80;
        unsigned char code = scancode & 0x7F;
        extended_key = 0;

        switch (code) {
            case 0x48: key_state[KEY_UP]    = !release; return;
            case 0x50: key_state[KEY_DOWN]  = !release; return;
            case 0x4D: key_state[KEY_RIGHT] = !release; return;
            case 0x4B: key_state[KEY_LEFT]  = !release; return;

            /* ===== Win (Left/Right) ===== */
            case 0x5B: /* Left Win  */
            case 0x5C: /* Right Win */
                win_pressed = !release;
                return;

            default: return;
        }
    }

    /* ===== Win + Space: تبديل اللغة ===== */
    if (scancode == 0x39 && win_pressed) {
        /* 0x39 = ضغط Space (بدون بت التحرير) */
        keyboard_language = (keyboard_language == KB_LANG_AR)
                            ? KB_LANG_EN : KB_LANG_AR;
        return;   /* لا نُدخل مسافة */
    }

    /* ===== Shift ===== */
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        key_state[KEY_LSHIFT] = 1;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        key_state[KEY_LSHIFT] = 0;
        return;
    }

    /* ===== تحديث حالة المفاتيح ===== */
    int release = scancode & 0x80;
    unsigned char code = scancode & 0x7F;
    if (code < 128)
        key_state[code] = !release;

    if (release) return;

    /* ===== إدخال الحرف / الحروف ===== */
    const char *s = lookup_utf8(code);
    if (s && *s)
        kb_push_utf8(s);
}

char keyboard_read() {
    if (kb_head == kb_tail) return 0;
    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return c;
}
