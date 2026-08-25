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

void keyboard_init(void)
{
    shift_pressed = 0;

    /* تصفير صريح — لا نعتمد على تصفير .bss الضمني وقت الإقلاع.
       على الهاردوير الحقيقي قد لا تكون هذه المنطقة صفراً فعلياً،
       وأي قيمة غير صفرية هنا تكسر الشرط (kb_head == kb_tail)
       وتؤدي لقراءة kb_buffer[kb_tail] بفهرس عشوائي خارج المخزن. */
    kb_head = 0;
    kb_tail = 0;

    wait_queue_init(&keyboard_queue);
}



void keyboard_handler() {

    unsigned char scancode = inb(0x60);

    // SHIFT PRESS
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }

    // SHIFT RELEASE
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return;
    }

    // تجاهل release
    if (scancode & 0x80)
        return;

    char c = shift_pressed ? keymap_shift[scancode] : keymap[scancode];

    if (c) {

        int next = (kb_head + 1) % KB_BUFFER_SIZE;

        //  منع overflow
        if (next != kb_tail) {
            kb_buffer[kb_head] = c;
            kb_head = next;

            wake_up_one(&keyboard_queue); //  يصحي العمليات المنتظرة
        }
    }

    last_key = c;
}

/* ================= READ (BLOCKING) ================= */

char keyboard_read() {

    if (kb_head == kb_tail)
        return 0;   //  مفيش حرف

    /* حماية دفاعية إضافية: masking صريح عند القراءة أيضاً، مش بس
       عند حساب القيمة الجديدة بعد الزيادة. حتى لو كان kb_tail لسبب
       ما خارج النطاق [0, KB_BUFFER_SIZE)، هذا يمنع قراءة عنوان خارج
       حدود kb_buffer بدل الاعتماد فقط على صحة القيمة القادمة. */
    char c = kb_buffer[kb_tail & (KB_BUFFER_SIZE - 1)];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;

    return c;
}
