#include "مؤشر_الفأرة.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مكتبة_واجهة_الإدخال_والإخراج.h"


int mouse_cycle = 0;
int8_t mouse_byte[3];

int mouse_x = 100;
int mouse_y = 100;
int mouse_left = 0;


// انتظار حتى يصبح Input Buffer فارغاً (جاهز لاستقبال أمر أو بيانات)
static void mouse_wait_input() {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(0x64) & 0x02) == 0) {
            return;
        }
    }
}

// انتظار حتى يصبح Output Buffer ممتلئاً (توجد بيانات جاهزة للقراءة)
static void mouse_wait_output() {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (inb(0x64) & 0x01) {
            return;
        }
    }
}

static void mouse_write(uint8_t port, uint8_t value) {
    mouse_wait_input();
    outb(port, value);
}

static uint8_t mouse_read_data() {
    mouse_wait_output();
    return inb(0x60);
}

void mouse_init() {

    // تفعيل المنفذ الثاني (الماوس) في متحكم 8042
    mouse_write(0x64, 0xA8);

    // قراءة byte الإعدادات الحالي
    mouse_write(0x64, 0x20);
    uint8_t status = mouse_read_data();

    // تفعيل مقاطعة IRQ12 (بت 1) وتفعيل ساعة المنفذ الثاني (مسح بت 5)
    status |= 0x02;
    status &= ~0x20;

    // كتابة byte الإعدادات الجديد
    mouse_write(0x64, 0x60);
    mouse_write(0x60, status);

    // إعادة الماوس لإعداداته الافتراضية (توافق أفضل مع العتاد الحقيقي)
    mouse_write(0x64, 0xD4);
    mouse_write(0x60, 0xF6);
    mouse_read_data(); // استهلاك ACK (0xFA)

    // تفعيل الإرسال التلقائي للبيانات (Data Reporting)
    mouse_write(0x64, 0xD4);
    mouse_write(0x60, 0xF4);
    mouse_read_data(); // استهلاك ACK (0xFA)
}

void mouse_handler() {

    uint8_t status = inb(0x64);

    // لا توجد بيانات جاهزة فعلياً في المخزن المؤقت
    if (!(status & 0x01)) {
        return;
    }

    // البيانات قادمة من لوحة المفاتيح وليس الماوس، تجاهلها هنا
    if (!(status & 0x20)) {
        inb(0x60);
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
