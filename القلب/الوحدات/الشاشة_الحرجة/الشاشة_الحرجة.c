#include "الشاشة_الحرجة.h"

#include "القلب/الوحدات/محرك_الفيديو/بوابة_الفيديو.h"
#include "القلب/الوحدات/محرك_الفيديو/محرك_الفيديو.h"

#include "محرك_العمليات/مدير_عمليات_النواه/مدير_عمليات_النواه.h"

/* ========================================================= */
/* إعدادات الشاشة والخط — عدّل القيم دي لو دقة الشاشة/الخط
   الفعليين مختلفين عندك */
/* ========================================================= */

#define PANIC_SCREEN_W 1024
#define PANIC_SCREEN_H 768
#define PANIC_CHAR_W   8
#define PANIC_CHAR_H   16

/* ========================================================= */
/* الشعار                                                     */
/* ========================================================= */

static const char* panic_logo[] = {
    "                                ########",
    "                         ######################",
    "                     ######                  ######",
    "                  #####                          #####",
    "                ####                                ####",
    "              ###                                      ###",
    "            ####                    #######             ###",
    "           ###                          #######           ###",
    "          ###                              ######          ###",
    "         ###               #                 ######         ###",
    "        ###             #####                 #######        ##",
    "        ##              #######         ##     #######        ##",
    "       ###             #########     #####      #######       ##",
    "       ##             ############ ###          #######       ###",
    "       ##              ################         ########       ##",
    "       ##              ################         ########       ##",
    "       ##               ############            ########       ##",
    "        ##                #######              #########      ##",
    "        ##               ########             #########       ##",
    "         ##     #        #####  ##           ##########      ##",
    "         ###     ##      ##                 ##########      ###",
    "          ###     ###                     ###########      ###",
    "           ###     #####              ##############      ###",
    "             ##      #############################       ##",
    "              ###      #########################       ###",
    "                ###       ##################         ###",
    "                  ####           #####            ####",
    "                     #####                    #####",
    "                         ######################",
    "                               #######"
};

#define PANIC_LOGO_LINE_COUNT (int)(sizeof(panic_logo) / sizeof(panic_logo[0]))

static int panic_strlen(const char* s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int panic_logo_max_width(void)
{
    int max_len = 0;
    for (int i = 0; i < PANIC_LOGO_LINE_COUNT; i++) {
        int len = panic_strlen(panic_logo[i]);
        if (len > max_len) max_len = len;
    }
    return max_len;
}

/* يرسم سطر بإزاحة أفقية محددة (مستخدم للشعار عشان شكله ميتكسرش) */
static void panic_draw_line_at(int x, int y, const char* text)
{
    draw_string(x, y, text, 0x00FFFFFF);
    video_api.swap();
}

/* يرسم سطر نص عادي ممركز أفقيًا لوحده (مستخدم لرسالة الـ panic) */
static void panic_draw_centered_line(int y, const char* text)
{
    int len = panic_strlen(text);
    int x = (PANIC_SCREEN_W - len * PANIC_CHAR_W) / 2;
    if (x < 0) x = 0;
    panic_draw_line_at(x, y, text);
}

/* ========================================================= */
/* PANIC SCREEN                                               */
/* ========================================================= */

static void panic_background()
{
    video_api.clear(0x00330000);
    video_api.swap();
}


/* ========================================================= */
/* HEX FORMAT (بيكتب في buffer بدل ما يرسم فورًا — عشان نجمع
   كل الأسطر الأول ونعرف ارتفاع الكتلة الكلي قبل ما نبدأ نرسم) */
/* ========================================================= */

static void panic_format_hex_u64(
    char* out,
    const char* label,
    uint64_t value
)
{
    char hexbuf[19];

    const char* hex =
        "0123456789ABCDEF";

    hexbuf[0] = '0';
    hexbuf[1] = 'x';

    for (int i = 0; i < 16; i++)
    {
        hexbuf[2 + i] =
            hex[
                (value >> ((15 - i) * 4))
                & 0xF
            ];
    }

    hexbuf[18] = '\0';

    strcpy(out, label);
    strcat(out, hexbuf);
}




static void panic_format_dec_u64(
    char* out,
    const char* label,
    uint64_t value
)
{
    char buf[24];
    int pos = 0;

    if (value == 0) {
        buf[pos++] = '0';
    } else {
        while (value) {
            buf[pos++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }

    char tmp[24];
    int j = 0;
    while (pos) tmp[j++] = buf[--pos];
    tmp[j] = '\0';

    strcpy(out, label);
    strcat(out, tmp);
}
/* ========================================================= */
/* EXCEPTION NAMES                                            */
/* ========================================================= */

static const char* exception_name(
    uint64_t vector
)
{
    switch (vector)
    {
        case 0:
            return "#DE Divide Error";

        case 6:
            return "#UD Invalid Opcode";

        case 8:
            return "#DF Double Fault";

        case 13:
            return "#GP General Protection";

        case 14:
            return "#PF Page Fault";

        default:
            return "Unknown Exception";
    }
}


/* ========================================================= */
/* KERNEL PANIC                                               */
/* ========================================================= */

static void kernel_panic_ex(
    const char* msg,
    const char** detail_lines,
    int detail_count
)
{
    asm volatile("cli");

    panic_background();

    /* بنجمع كل أسطر النص (أسفل الشعار) في مصفوفة واحدة قبل ما نرسم
       أي حاجة، عشان نقدر نحسب الارتفاع الكلي ونركّز الكتلة رأسيًا */
    const char* text_lines[16];
    int n = 0;

    text_lines[n++] = "====================";
    text_lines[n++] = " KERNEL ERROR !*_*! ";
    text_lines[n++] = "====================";
    text_lines[n++] = "";

    for (int i = 0; i < detail_count; i++) {
        text_lines[n++] = detail_lines[i];
    }
    if (detail_count > 0) {
        text_lines[n++] = "";
    }

    text_lines[n++] = msg;
    text_lines[n++] = "";
    text_lines[n++] = "System halted";

    const int gap_lines = 1; /* فراغ بين الشعار والنص */

    int total_lines  = PANIC_LOGO_LINE_COUNT + gap_lines + n;
    int total_height = total_lines * PANIC_CHAR_H;

    int y = (PANIC_SCREEN_H - total_height) / 2;
    if (y < 0) y = 0;

    /* رسم الشعار — إزاحة أفقية واحدة ثابتة لكل الأسطر عشان الشكل يفضل سليم */
    int logo_x = (PANIC_SCREEN_W - panic_logo_max_width() * PANIC_CHAR_W) / 2;
    if (logo_x < 0) logo_x = 0;

    for (int i = 0; i < PANIC_LOGO_LINE_COUNT; i++) {
        panic_draw_line_at(logo_x, y, panic_logo[i]);
        y += PANIC_CHAR_H;
    }

    y += gap_lines * PANIC_CHAR_H;

    /* رسم النص — كل سطر ممركز لوحده */
    for (int i = 0; i < n; i++) {
        panic_draw_centered_line(y, text_lines[i]);
        y += PANIC_CHAR_H;
    }

    while (1)
    {
        asm volatile("hlt");
    }
}

void kernel_panic(
    const char* msg
)
{
    kernel_panic_ex(msg, 0, 0);
}


/* ========================================================= */
/* PAGE FAULT                                                 */
/* ========================================================= */

void panic_page_fault(
    uint64_t addr,
    uint64_t* stack
)
{
    char line_cr2[96];
    char line_err[96];
    char line_rip[96];

    const char* detail_lines[8];
    int detail_count = 0;

    detail_lines[detail_count++] = "PAGE FAULT";
    detail_lines[detail_count++] = "";

    panic_format_hex_u64(line_cr2, "CR2: ", addr);
    detail_lines[detail_count++] = line_cr2;

    if (stack)
    {
        panic_format_hex_u64(line_err, "Error code: ", stack[15]);
        detail_lines[detail_count++] = line_err;

        panic_format_hex_u64(line_rip, "RIP: ", stack[16]);
        detail_lines[detail_count++] = line_rip;
    }

    kernel_panic_ex(
        "Invalid memory access",
        detail_lines,
        detail_count
    );
}


/* ========================================================= */
/* GENERAL EXCEPTION                                          */
/* ========================================================= */

void generic_exception_handler_c(
    uint64_t* stack
)
{
    uint64_t vector =
        stack[15];

    uint64_t error_code =
        stack[16];

    uint64_t rip =
        stack[17];

    char line_vector[96];
    char line_err[96];
    char line_rip[96];
    char line_pid[96];
    char line_rsp[96];

    panic_format_hex_u64(line_vector, "Vector: ", vector);
    panic_format_hex_u64(line_err, "Error code: ", error_code);
    panic_format_hex_u64(line_rip, "RIP: ", rip);

    process_t* current = kernel_process_current();

    if (current) {
        panic_format_dec_u64(line_pid, "PID: ", (uint64_t)current->pid);
        panic_format_hex_u64(line_rsp, "Saved RSP: ", current->rsp);
    } else {
        strcpy(line_pid, "PID: (none / kernel context)");
        strcpy(line_rsp, "");
    }

    const char* detail_lines[8];
    int detail_count = 0;

    detail_lines[detail_count++] = exception_name(vector);
    detail_lines[detail_count++] = "";
    detail_lines[detail_count++] = line_vector;
    detail_lines[detail_count++] = line_err;
    detail_lines[detail_count++] = line_rip;
    detail_lines[detail_count++] = line_pid;
    detail_lines[detail_count++] = line_rsp;

    kernel_panic_ex(
        "Unhandled CPU exception",
        detail_lines,
        detail_count
    );
}
