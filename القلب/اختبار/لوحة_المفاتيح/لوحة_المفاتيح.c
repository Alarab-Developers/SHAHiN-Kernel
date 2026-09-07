#include "لوحة_المفاتيح.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مكتبة_واجهة_الإدخال_والإخراج.h"
#include "محرك_الجدولة/الأدارة/مُدير_قائمة_الأنتظار.h"

wait_queue_t keyboard_queue;

#define KB_BUFFER_SIZE 128

char kb_buffer[KB_BUFFER_SIZE];
int kb_head = 0;
int kb_tail = 0;
volatile char last_key = 0;

static int shift_pressed = 0;
static volatile unsigned char key_state[128] = {0};  /* الآن 128 مدخلاً */
static int extended_key = 0;

int keyboard_is_pressed(unsigned char key) {
    if (key >= 128)
        return 0;
    return key_state[key];
}

static const char keymap[128] = {
  0,27,'1','2','3','4','5','6','7','8',
 '9','0','-','=', '\b',
 '\t',
 'q','w','e','r','t','y','u','i','o','p',
 '[',']','\n',
 0,
 'a','s','d','f','g','h','j','k','l',
 ';','\'','`',
 0,
 '\\',
 'z','x','c','v','b','n','m',
 ',','.','/',
 0,'*',0,' '
};

static const char keymap_shift[128] = {
  0,27,'!','@','#','$','%','^','&','*',
 '(',')','_','+', '\b',
 '\t',
 'Q','W','E','R','T','Y','U','I','O','P',
 '{','}','\n',
 0,
 'A','S','D','F','G','H','J','K','L',
 ':','"','~',
 0,
 '|',
 'Z','X','C','V','B','N','M',
 '<','>','?',
 0,'*',0,' '
};

char keyboard_getchar() {
    while (!(inb(0x64) & 1));
    unsigned char scancode = inb(0x60);
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return 0;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return 0;
    }
    if (scancode & 0x80)
        return 0;
    if (scancode >= 128)
        return 0;
    if (shift_pressed)
        return keymap_shift[scancode];
    return keymap[scancode];
}

void keyboard_init(void) {
    shift_pressed = 0;
    kb_head = 0;
    kb_tail = 0;
    for (int i = 0; i < 128; i++)
        key_state[i] = 0;
    wait_queue_init(&keyboard_queue);
}

void keyboard_handler() {
    unsigned char scancode = inb(0x60);

    /* مفتاح Extended */
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
            /* يمكن إضافة مفاتيح Extended أخرى هنا */
            default: return;
        }
    }

    /* SHIFT */
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

    /* تحديث حالة المفتاح (ضغط/تحرير) للمفاتيح العادية */
    int release = scancode & 0x80;
    unsigned char code = scancode & 0x7F;
    if (code < 128) {
        key_state[code] = !release;
    }

    /* تجاهل التحرير لإدخال الحروف */
    if (release)
        return;

    /* إضافة الحرف إلى المخزن المؤقت إن أمكن */
    char c = shift_pressed ? keymap_shift[code] : keymap[code];
    if (c) {
        int next = (kb_head + 1) % KB_BUFFER_SIZE;
        if (next != kb_tail) {
            kb_buffer[kb_head] = c;
            kb_head = next;
            wake_up_one(&keyboard_queue);
        }
    }
    last_key = c;
}

char keyboard_read() {
    if (kb_head == kb_tail)
        return 0;
    char c = kb_buffer[kb_tail & (KB_BUFFER_SIZE - 1)];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return c;
}
