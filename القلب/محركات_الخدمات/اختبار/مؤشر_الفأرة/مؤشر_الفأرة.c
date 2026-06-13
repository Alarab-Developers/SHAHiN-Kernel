#include "مؤشر_الفأرة.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مكتبة_واجهة_الإدخال_والإخراج.h"


int mouse_cycle = 0;
int8_t mouse_byte[3];

int mouse_x = 100;
int mouse_y = 100;
int mouse_left = 0;

void mouse_init() {

    // تفعيل الماوس
    outb(0x64, 0xA8);

    // تفعيل IRQ12
    outb(0x64, 0x20);
    uint8_t status = inb(0x60);
    status |= 2;
    outb(0x64, 0x60);
    outb(0x60, status);

    // إرسال أمر البدء
    outb(0x64, 0xD4);
    outb(0x60, 0xF4);
}

void mouse_handler() {

    uint8_t status = inb(0x64);

    if (!(status & 0x20)) {
        return;
    }

    uint8_t data = inb(0x60);

    mouse_byte[mouse_cycle++] = data;

    if (mouse_cycle == 1) {
        if (!(mouse_byte[0] & 0x08)) {
            mouse_cycle = 0;
        }
    }

    if (mouse_cycle == 3) {

        int dx = (int8_t)mouse_byte[1];
        int dy = (int8_t)mouse_byte[2];

        mouse_x += dx;
        mouse_y -= dy;

        mouse_left = mouse_byte[0] & 0x1;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;

        mouse_cycle = 0;
    }
}

