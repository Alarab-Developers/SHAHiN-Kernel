#include "مؤشر_الفأرة.h"
#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مكتبة_واجهة_الإدخال_والإخراج.h"
#include "القلب/الوحدات/محرك_الفيديو/بوابة_الفيديو.h"


int mouse_cycle = 0;
int8_t mouse_byte[3];

int mouse_x = 100;
int mouse_y = 100;
int mouse_left = 0;


/* ===== حدث النقر ===== */
int mouse_left_clicked = 0;      /* 1 إذا وُجدت نقرة لم تُقرأ بعد */
static int mouse_left_prev = 0;  /* حالة الزر في الحزمة السابقة */

/*
 * حركة الماوس المتراكمة منذ آخر قراءة
 * من قبل برنامج اللغة.
 *
 * mouse_dx = حركة محور س
 * mouse_dy = حركة محور ص
 */
int mouse_dx = 0;
int mouse_dy = 0;


// انتظار حتى يصبح Input Buffer فارغاً
// جاهز لاستقبال أمر أو بيانات
static void mouse_wait_input()
{
    uint32_t timeout = 100000;

    while (timeout--)
    {
        if ((inb(0x64) & 0x02) == 0)
        {
            return;
        }
    }
}


// انتظار حتى يصبح Output Buffer ممتلئاً
// توجد بيانات جاهزة للقراءة
static void mouse_wait_output()
{
    uint32_t timeout = 100000;

    while (timeout--)
    {
        if (inb(0x64) & 0x01)
        {
            return;
        }
    }
}


static void mouse_write(
    uint16_t port,
    uint8_t value
)
{
    mouse_wait_input();
    outb(port, value);
}


static uint8_t mouse_read_data()
{
    mouse_wait_output();
    return inb(0x60);
}


void mouse_init()
{
    /*
     * تصفير حالة تحليل الحزمة
     */
    mouse_cycle = 0;

    mouse_byte[0] = 0;
    mouse_byte[1] = 0;
    mouse_byte[2] = 0;


    /*
     * تصفير الحركة
     */
    mouse_dx = 0;
    mouse_dy = 0;


    /*
     * الموضع الابتدائي: منتصف الشاشة
     */
    {
        uint32_t شاشة_عرض    = video_api.width();
        uint32_t شاشة_ارتفاع = video_api.height();

        mouse_x = شاشة_عرض    ? (int)(شاشة_عرض    / 2) : 400;
        mouse_y = شاشة_ارتفاع ? (int)(شاشة_ارتفاع / 2) : 300;
    }


    /*
     * حالة الزر
     */
    mouse_left = 0;


    /*

    /*
     * حدث النقر وحالة الزر السابقة
     */
    mouse_left_clicked = 0;
    mouse_left_prev    = 0;
    /*
     * تفعيل المنفذ الثاني
     * في متحكم 8042
     */
    mouse_write(
        0x64,
        0xA8
    );


    /*
     * قراءة Byte الإعدادات الحالي
     */
    mouse_write(
        0x64,
        0x20
    );

    uint8_t status =
        mouse_read_data();


    /*
     * تفعيل IRQ12
     *
     * البت 1 = 1
     *
     * وتفعيل ساعة المنفذ الثاني
     *
     * البت 5 = 0
     */
    status |= 0x02;
    status &= (uint8_t)~0x20;


    /*
     * كتابة Byte الإعدادات
     */
    mouse_write(
        0x64,
        0x60
    );

    mouse_write(
        0x60,
        status
    );


    /*
     * إعادة الماوس إلى إعداداته الافتراضية
     */
    mouse_write(
        0x64,
        0xD4
    );

    mouse_write(
        0x60,
        0xF6
    );


    /*
     * استهلاك ACK
     */
    mouse_read_data();


    /*
     * تفعيل الإرسال التلقائي للبيانات
     *
     * Data Reporting
     */
    mouse_write(
        0x64,
        0xD4
    );

    mouse_write(
        0x60,
        0xF4
    );


    /*
     * استهلاك ACK
     */
    mouse_read_data();
}


void mouse_handler()
{
    uint8_t status =
        inb(0x64);


    /*
     * لا توجد بيانات جاهزة
     */
    if (!(status & 0x01))
    {
        return;
    }


    /*
     * البيانات من لوحة المفاتيح
     * وليست من الماوس
     */
    if (!(status & 0x20))
    {
        inb(0x60);
        return;
    }


    /*
     * قراءة Byte من الماوس
     */
    uint8_t data =
        inb(0x60);


    /*
     * حماية إضافية من تجاوز المصفوفة
     */
    if (mouse_cycle < 0 || mouse_cycle >= 3)
    {
        mouse_cycle = 0;
    }


    /*
     * تخزين Byte في الحزمة
     */
    mouse_byte[mouse_cycle] =
        data;

    mouse_cycle++;


    /*
     * أول Byte يجب أن يحتوي على
     * البت 3
     */
    if (mouse_cycle == 1)
    {
        if (!(mouse_byte[0] & 0x08))
        {
            mouse_cycle = 0;
            return;
        }
    }


    /*
     * اكتملت حزمة الماوس
     */
    if (mouse_cycle == 3)
    {
        /*
         * حركة X لهذه الحزمة فقط
         */
        int dx =
            (int8_t)mouse_byte[1];


        /*
         * حركة Y لهذه الحزمة
         *
         * نقلب الاتجاه ليتوافق
         * مع نظام إحداثيات الشاشة
         */
        int dy =
            -(int8_t)mouse_byte[2];


        /*
         * إضافة الحركة إلى الحركة
         * غير المستهلكة من قبل اللغة.
         */
        mouse_dx += dx;
        mouse_dy += dy;


        /*
         * تحديث الموضع باستخدام الحركة
         * الجديدة فقط، وليس الحركة المتراكمة.
         */
        mouse_x += dx;
        mouse_y += dy;


        /*
         * قراءة زر الماوس الأيسر
         */
        mouse_left =
            mouse_byte[0] & 0x01;

        /*
         * كشف حافة الصعود (0 → 1)
         * = حدث نقرة جديدة
         */
        if (mouse_left && !mouse_left_prev)
        {
            mouse_left_clicked = 1;
        }

        mouse_left_prev = mouse_left;


        /*
         * أبعاد الشاشة
         */
        int screen_w =
            video_api.width();

        int screen_h =
            video_api.height();


        /*
         * الحد الأدنى
         */
        if (mouse_x < 0)
        {
            mouse_x = 0;
        }

        if (mouse_y < 0)
        {
            mouse_y = 0;
        }


        /*
         * الحد الأقصى
         */
        if (screen_w > 0 &&
            mouse_x >= screen_w)
        {
            mouse_x =
                screen_w - 1;
        }

        if (screen_h > 0 &&
            mouse_y >= screen_h)
        {
            mouse_y =
                screen_h - 1;
        }


        /*
         * بدء حزمة جديدة
         */
        mouse_cycle = 0;
    }
}


int mouse_consume_click(void)
{
    int c = mouse_left_clicked;
    mouse_left_clicked = 0;
    return c;
}
