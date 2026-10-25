#include "انواع.h"
#include "المكتبات.h"

/* ====== ثوابت Syscall (شاهين x86_64) ====== */
#define SYS_READ   0
#define SYS_WRITE  1
#define SYS_OPEN   2
#define SYS_CLOSE  3
#define SYS_LSEEK  8
#define SYS_EXIT   60

#define O_RDONLY   0
#define O_WRONLY   1
#define O_CREAT    0100
#define O_TRUNC    01000

#define SEEK_SET   0
#define SEEK_END   2

/* ====== ثوابت المحول ====== */
#define KERNEL_FORMAT     "[نواة]"
#define KERNEL_LOAD_BASE  0x100000ULL
#define PAGE_ALIGN        0x1000ULL

typedef struct
{
    char     format[128];
    uint64_t entry_point;
    uint64_t image_size;
    uint64_t flags;
} aros_kernel_header_t;

/* ====== Syscall wrappers ====== */
static inline long syscall3(long n, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long syscall1(long n, long a1)
{
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static int sys_open(const char *path, int flags, int mode)
{
    return (int)syscall3(SYS_OPEN, (long)path, flags, mode);
}

static long sys_read(int fd, void *buf, long count)
{
    return syscall3(SYS_READ, fd, (long)buf, count);
}

static long sys_write(int fd, const void *buf, long count)
{
    return syscall3(SYS_WRITE, fd, (long)buf, count);
}

static long sys_lseek(int fd, long offset, int whence)
{
    return syscall3(SYS_LSEEK, fd, offset, whence);
}

static long sys_close(int fd)
{
    return syscall1(SYS_CLOSE, fd);
}

/* ====== طباعة بسيطة ====== */
typedef __builtin_va_list va_list;
#define va_start(v,l) __builtin_va_start(v,l)
#define va_arg(v,l)   __builtin_va_arg(v,l)
#define va_end(v)     __builtin_va_end(v)

static void print_str_fd(int fd, const char *s)
{
    if (!s) s = "(null)";
    sys_write(fd, s, (long)strlen(s));
}

static void print_char_fd(int fd, char c)
{
    sys_write(fd, &c, 1);
}

static void print_hex_fd(int fd, uint64_t v)
{
    char buf[16];
    int i = 16;
    if (v == 0) {
        print_char_fd(fd, '0');
        return;
    }
    while (v && i > 0) {
        int d = v & 0xF;
        buf[--i] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        v >>= 4;
    }
    sys_write(fd, buf + i, 16 - i);
}

static void print_dec_fd(int fd, uint64_t v)
{
    char buf[20];
    int i = 20;
    if (v == 0) {
        print_char_fd(fd, '0');
        return;
    }
    while (v && i > 0) {
        buf[--i] = '0' + (v % 10);
        v /= 10;
    }
    sys_write(fd, buf + i, 20 - i);
}

static void print_signed_dec_fd(int fd, int64_t v)
{
    if (v < 0) {
        print_char_fd(fd, '-');
        print_dec_fd(fd, (uint64_t)(-v));
    } else {
        print_dec_fd(fd, (uint64_t)v);
    }
}

static int mini_printf(int fd, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            print_char_fd(fd, *fmt++);
            continue;
        }

        fmt++; /* تجاوز % */

        int long_count = 0;
        while (*fmt == 'l') {
            long_count++;
            fmt++;
        }

        switch (*fmt) {
        case 'x': {
            uint64_t v;
            if (long_count >= 2) v = va_arg(ap, unsigned long long);
            else if (long_count == 1) v = va_arg(ap, unsigned long);
            else v = va_arg(ap, unsigned int);
            print_hex_fd(fd, v);
            break;
        }
        case 'u': {
            uint64_t v;
            if (long_count >= 2) v = va_arg(ap, unsigned long long);
            else if (long_count == 1) v = va_arg(ap, unsigned long);
            else v = va_arg(ap, unsigned int);
            print_dec_fd(fd, v);
            break;
        }
        case 'd':
        case 'i': {
            int64_t v;
            if (long_count >= 2) v = va_arg(ap, long long);
            else if (long_count == 1) v = va_arg(ap, long);
            else v = va_arg(ap, int);
            print_signed_dec_fd(fd, v);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            print_str_fd(fd, s);
            break;
        }
        case 'c': {
            int c = va_arg(ap, int);
            print_char_fd(fd, (char)c);
            break;
        }
        case '%':
            print_char_fd(fd, '%');
            break;
        default:
            print_char_fd(fd, '%');
            print_char_fd(fd, *fmt);
            break;
        }

        if (*fmt) fmt++;
    }

    va_end(ap);
    return 0;
}

/* ====== strtoull بسيط ====== */
static uint64_t simple_strtoull(const char *s, char **endptr, int base)
{
    if (!s) {
        if (endptr) *endptr = (char *)s;
        return 0;
    }

    /* تجاهل الفراغات */
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;

    if (base == 0) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            base = 16;
            s += 2;
        } else if (s[0] == '0') {
            base = 8;
            s++;
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    }

    uint64_t val = 0;
    while (*s) {
        int digit;
        char c = *s;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else break;

        if (digit >= base) break;
        val = val * base + digit;
        s++;
    }

    if (endptr) *endptr = (char *)s;
    return val;
}

/* ====== نقطة الدخول ====== */
int main(int argc, char **argv);

__attribute__((naked, noreturn)) void _start(void)
{
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "andq $-16, %rsp\n"
        "call main\n"
        "mov %eax, %edi\n"
        "mov $60, %eax\n"
        "syscall\n"
    );
}

int main(int argc, char *argv[])
{
    uint64_t entry_offset = 0x53b;
    uint64_t kernel_end   = 0;

    if (argc >= 2) {
        uint64_t abs_addr = simple_strtoull(argv[1], NULL, 0);
        if (abs_addr >= KERNEL_LOAD_BASE) {
            entry_offset = abs_addr - KERNEL_LOAD_BASE;
        } else {
            entry_offset = abs_addr;
        }
    }

    if (argc >= 3) {
        kernel_end = simple_strtoull(argv[2], NULL, 0);
    }

    mini_printf(1, "[maker] KERNEL_LOAD_BASE = 0x%llx\n",
                (unsigned long long)KERNEL_LOAD_BASE);
    mini_printf(1, "[maker] entry_offset     = 0x%llx\n",
                (unsigned long long)entry_offset);
    mini_printf(1, "[maker] kernel_end       = 0x%llx\n",
                (unsigned long long)kernel_end);

    int in = sys_open("kernel.bin", O_RDONLY, 0);
    if (in < 0) {
        mini_printf(2, "[ERROR] لا يمكن فتح kernel.bin\n");
        return 1;
    }

    int out = sys_open("شاهين.نواة", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) {
        mini_printf(2, "[ERROR] لا يمكن إنشاء شاهين.نواة\n");
        sys_close(in);
        return 1;
    }

    long file_size = sys_lseek(in, 0, SEEK_END);
    sys_lseek(in, 0, SEEK_SET);

    uint64_t image_size = (uint64_t)file_size;

    if (kernel_end > KERNEL_LOAD_BASE) {
        uint64_t from_end = kernel_end - KERNEL_LOAD_BASE;
        if (from_end > image_size)
            image_size = from_end;
    } else {
        mini_printf(2,
            "[WARN] لم يتم تمرير _kernel_end.\n"
            "       ستُبنى جداول الصفحات عند 0x%llx على الأرجح،\n"
            "       وقد تتداخل مع .bss وتسبب Triple Fault.\n"
            "       مرّر _kernel_end كوسيط ثانٍ.\n",
            (unsigned long long)(KERNEL_LOAD_BASE + image_size + PAGE_ALIGN));
    }

    image_size = (image_size + PAGE_ALIGN - 1) & ~(PAGE_ALIGN - 1);

    mini_printf(1, "[maker] kernel.bin size  = %ld bytes\n", file_size);
    mini_printf(1, "[maker] image_size       = %llu bytes (0x%llx)\n",
                (unsigned long long)image_size,
                (unsigned long long)image_size);
    mini_printf(1, "[maker] kernel spans     0x%llx .. 0x%llx\n",
                (unsigned long long)KERNEL_LOAD_BASE,
                (unsigned long long)(KERNEL_LOAD_BASE + image_size));
    mini_printf(1, "[maker] bootloader will place page tables at 0x%llx\n",
                (unsigned long long)(KERNEL_LOAD_BASE + image_size));

    aros_kernel_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    strcpy(hdr.format, KERNEL_FORMAT);
    hdr.entry_point = entry_offset;
    hdr.image_size  = image_size;
    hdr.flags       = 0;

    sys_write(out, &hdr, sizeof(hdr));

    char buf[4096];
    uint64_t copied = 0;
    while (copied < (uint64_t)file_size) {
        long n = sys_read(in, buf, sizeof(buf));
        if (n <= 0) break;
        long written = sys_write(out, buf, n);
        if (written != n) {
            mini_printf(2, "[ERROR] فشل كتابة البيانات\n");
            sys_close(in);
            sys_close(out);
            return 1;
        }
        copied += n;
    }
    sys_close(in);

    static const char zeros[4096] = {0};
    while (copied < image_size) {
        uint64_t remain = image_size - copied;
        long n = (remain < sizeof(zeros)) ? (long)remain : (long)sizeof(zeros);
        long written = sys_write(out, zeros, n);
        if (written != n) {
            mini_printf(2, "[ERROR] فشل كتابة padding\n");
            sys_close(out);
            return 1;
        }
        copied += n;
    }

    sys_close(out);

    mini_printf(1, "[maker] شاهين.نواة تم إنشاؤها بنجاح (%llu بايت إجمالًا)\n",
                (unsigned long long)(sizeof(hdr) + image_size));
    return 0;
}
