#include "مدير_الطاقة.h"
#include "مكتبة_واجهة_الإدخال_والإخراج.h"



#define PHYS_TO_VIRT(phys) ((void *)(uintptr_t)(phys))


__attribute__((weak)) void power_debug_checkpoint(uint8_t code)
{
    outb(0x80, code);
}

__attribute__((weak)) void power_ensure_mapped(uint64_t phys_addr, uint64_t length)
{
    (void)phys_addr;
    (void)length;
}

/* رموز نقاط التفتيش - راجعها في التعليقات عند كل استدعاء */
enum {
    CP_INIT_ENTER          = 0x01,
    CP_RSDP_SIG_OK         = 0x02,
    CP_RSDP_CHECKSUM_OK    = 0x03,
    CP_FADT_FOUND          = 0x04,
    CP_DSDT_MAPPED         = 0x05,
    CP_S5_PARSED_OK        = 0x06,
    CP_RESET_REG_CAPTURED  = 0x07,
    CP_INIT_DONE           = 0x08,

    CP_SHUTDOWN_ENTER      = 0x10,
    CP_SHUTDOWN_ACPI_WRITE = 0x11,
    CP_SHUTDOWN_STILL_ALIVE_AFTER_ACPI = 0x12, /* لو ظهر هذا: الكتابة تمت لكن الجهاز لم يُطفأ */
    CP_SHUTDOWN_FALLBACK_QEMU_PORTS    = 0x13,
    CP_SHUTDOWN_FINAL_HALT             = 0x14, /* لو توقفت هنا فورًا: g_acpi_ready=0 */
};

/* ---------------------------------------------------------------------
 * هياكل ACPI الأساسية
 * ------------------------------------------------------------------- */

typedef struct {
    char     Signature[8];
    uint8_t  Checksum;
    char     OEMID[6];
    uint8_t  Revision;
    uint32_t RsdtAddress;
    /* ACPI 2.0+ */
    uint32_t Length;
    uint64_t XsdtAddress;
    uint8_t  ExtendedChecksum;
    uint8_t  Reserved[3];
} __attribute__((packed)) rsdp_t;

typedef struct {
    char     Signature[4];
    uint32_t Length;
    uint8_t  Revision;
    uint8_t  Checksum;
    char     OEMID[6];
    char     OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} __attribute__((packed)) sdt_header_t;

typedef struct {
    uint8_t  AddressSpace;
    uint8_t  BitWidth;
    uint8_t  BitOffset;
    uint8_t  AccessSize;
    uint64_t Address;
} __attribute__((packed)) gas_t;

typedef struct {
    sdt_header_t h;
    uint32_t FirmwareCtrl;
    uint32_t Dsdt;
    uint8_t  Reserved0;
    uint8_t  PreferredPMProfile;
    uint16_t SciInterrupt;
    uint32_t SmiCommandPort;
    uint8_t  AcpiEnable;
    uint8_t  AcpiDisable;
    uint8_t  S4BiosReq;
    uint8_t  PstateControl;
    uint32_t Pm1aEventBlock;
    uint32_t Pm1bEventBlock;
    uint32_t Pm1aControlBlock;
    uint32_t Pm1bControlBlock;
    uint32_t Pm2ControlBlock;
    uint32_t PmTimerBlock;
    uint32_t Gpe0Block;
    uint32_t Gpe1Block;
    uint8_t  Pm1EventLength;
    uint8_t  Pm1ControlLength;
    uint8_t  Pm2ControlLength;
    uint8_t  PmTimerLength;
    uint8_t  Gpe0Length;
    uint8_t  Gpe1Length;
    uint8_t  Gpe1Base;
    uint8_t  CStateControl;
    uint16_t WorstC2Latency;
    uint16_t WorstC3Latency;
    uint16_t FlushSize;
    uint16_t FlushStride;
    uint8_t  DutyOffset;
    uint8_t  DutyWidth;
    uint8_t  DayAlarm;
    uint8_t  MonthAlarm;
    uint8_t  Century;
    uint16_t BootArchFlags;
    uint8_t  Reserved1;
    uint32_t Flags;
    gas_t    ResetReg;
    uint8_t  ResetValue;
    uint8_t  Reserved2[3];
    uint64_t XFirmwareCtrl;
    uint64_t XDsdt;

} __attribute__((packed)) fadt_t;

#define FADT_FLAG_RESET_REG_SUPPORTED (1u << 10)


static uint8_t  g_acpi_ready       = 0;
static uint16_t g_pm1a_cnt_port    = 0;
static uint16_t g_pm1b_cnt_port    = 0; /* قد تكون 0 إن لم يوجد PM1b */
static uint16_t g_slp_typa         = 0;
static uint16_t g_slp_typb         = 0;

static uint8_t  g_reset_ready      = 0;
static uint16_t g_reset_port       = 0;
static uint8_t  g_reset_value      = 0;

#define SLP_EN (1u << 13)

/* ---------------------------------------------------------------------
 * أدوات مساعدة صغيرة (بدون الاعتماد على string.h)
 * ------------------------------------------------------------------- */

static int mem_eq(const void *a, const void *b, size_t n)
{
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (size_t i = 0; i < n; i++)
        if (pa[i] != pb[i])
            return 0;
    return 1;
}

static uint8_t sum_bytes(const uint8_t *p, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++)
        sum = (uint8_t)(sum + p[i]);
    return sum;
}

static const uint8_t *find_pattern(const uint8_t *hay, size_t hay_len,
                                    const uint8_t *needle, size_t needle_len)
{
    if (needle_len == 0 || hay_len < needle_len)
        return NULL;
    for (size_t i = 0; i + needle_len <= hay_len; i++) {
        if (mem_eq(hay + i, needle, needle_len))
            return hay + i;
    }
    return NULL;
}

static void power_io_wait(void)
{
    outb(0x80, 0);
}

/* ---------------------------------------------------------------------
 * البحث عن جدول SDT بتوقيع معين داخل RSDT أو XSDT
 * ------------------------------------------------------------------- */

static sdt_header_t *find_table_rsdt(uint32_t rsdt_phys, const char sig[4])
{
    /* المرحلة 1: خطّط رأس الجدول فقط لقراءة Length بأمان */
    power_ensure_mapped(rsdt_phys, sizeof(sdt_header_t));
    sdt_header_t *rsdt = (sdt_header_t *)PHYS_TO_VIRT(rsdt_phys);
    uint32_t total_len = rsdt->Length;

    /* المرحلة 2: خطّط الجدول كاملًا الآن بعد معرفة طوله الحقيقي */
    power_ensure_mapped(rsdt_phys, total_len);

    uint32_t entries = (total_len - (uint32_t)sizeof(sdt_header_t)) / 4;
    uint32_t *table = (uint32_t *)((uint8_t *)rsdt + sizeof(sdt_header_t));

    for (uint32_t i = 0; i < entries; i++) {
        power_ensure_mapped(table[i], sizeof(sdt_header_t));
        sdt_header_t *hdr = (sdt_header_t *)PHYS_TO_VIRT(table[i]);
        if (mem_eq(hdr->Signature, sig, 4)) {
            power_ensure_mapped(table[i], hdr->Length);
            return hdr;
        }
    }
    return NULL;
}

static sdt_header_t *find_table_xsdt(uint64_t xsdt_phys, const char sig[4])
{
    power_ensure_mapped(xsdt_phys, sizeof(sdt_header_t));
    sdt_header_t *xsdt = (sdt_header_t *)PHYS_TO_VIRT(xsdt_phys);
    uint32_t total_len = xsdt->Length;

    power_ensure_mapped(xsdt_phys, total_len);

    uint32_t entries = (total_len - (uint32_t)sizeof(sdt_header_t)) / 8;
    uint64_t *table = (uint64_t *)((uint8_t *)xsdt + sizeof(sdt_header_t));

    for (uint32_t i = 0; i < entries; i++) {
        power_ensure_mapped(table[i], sizeof(sdt_header_t));
        sdt_header_t *hdr = (sdt_header_t *)PHYS_TO_VIRT(table[i]);
        if (mem_eq(hdr->Signature, sig, 4)) {
            power_ensure_mapped(table[i], hdr->Length);
            return hdr;
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------------
 * استخراج SLP_TYPa / SLP_TYPb من الكائن ‎_S5‎ داخل DSDT
 *
 * هذه هي الحيلة المعروفة والمستخدمة في عدد كبير من أنظمة التشغيل
 * الهاوية لتفادي كتابة مُفسّر AML كامل: نبحث مباشرة عن النص "_S5_"
 * ثم نتخطى بايتات ترميز PkgLength وبايت عدد العناصر حسب مواصفة ACPI
 * (القسم 20.2.4)، ثم نقرأ قيمتي SLP_TYPa و SLP_TYPb.
 *
 * تنبيه: هذا الأسلوب يعمل مع الغالبية العظمى من جداول DSDT، لكنه ليس
 * مُفسّر AML حقيقي. إن فشل (أعاد 0 مع طباعة خطأ) على جهاز معيّن، فالحل
 * الجذري هو تفسير AML كاملًا (أو استخدام مكتبة مثل ACPICA)./
 * ------------------------------------------------------------------- */

static int parse_s5(const sdt_header_t *dsdt)
{
    const uint8_t *data = (const uint8_t *)dsdt;
    size_t len = dsdt->Length;
    const uint8_t needle[4] = { '_', 'S', '5', '_' };

    const uint8_t *p = find_pattern(data, len, needle, 4);
    if (!p)
        return 0;

    p += 4; /* تخطي "_S5_" */

    /* قد يسبق PkgLength بايت PackageOp = 0x12 */
    if (*p == 0x12)
        p++;

    /* تخطي ترميز PkgLength (قسم 20.2.4 من مواصفة ACPI) */
    uint8_t lead = *p;
    uint8_t extra = (lead >> 6) & 0x3;
    if (extra == 0)
        p += 1;
    else
        p += 1 + extra;

    /* تخطي بايت "عدد العناصر" */
    p += 1;

    /* العنصر الأول: SLP_TYPa (قد يُسبق بـ BytePrefix = 0x0A) */
    if (*p == 0x0A)
        p++;
    g_slp_typa = (uint16_t)(*p) << 10;
    p++;

    /* العنصر الثاني: SLP_TYPb */
    if (*p == 0x0A)
        p++;
    g_slp_typb = (uint16_t)(*p) << 10;

    return 1;
}

/* ---------------------------------------------------------------------
 * power_init: يُستدعى مرة واحدة أثناء الإقلاع بعد تفعيل الترقيم
 * ------------------------------------------------------------------- */

void power_init(void *rsdp_physical_address)
{
    power_debug_checkpoint(CP_INIT_ENTER);

    if (!rsdp_physical_address)
        return;

    uint64_t rsdp_phys = (uint64_t)(uintptr_t)rsdp_physical_address;

    /* خطّط منطقة RSDP قبل قراءتها - هذا هو المكان الأرجح للتجمد إن كان
       العنوان يقع خارج النطاق المُخطَّط حاليًا */
    power_ensure_mapped(rsdp_phys, sizeof(rsdp_t));
    rsdp_t *rsdp = (rsdp_t *)PHYS_TO_VIRT(rsdp_phys);

    if (!mem_eq(rsdp->Signature, "RSD PTR ", 8))
        return; /* لو توقفت هنا: العنوان يشير لمكان خاطئ في الذاكرة */
    power_debug_checkpoint(CP_RSDP_SIG_OK);

    if (sum_bytes((uint8_t *)rsdp, 20) != 0)
        return; /* checksum فاشل - البيانات موجودة لكنها ليست RSDP صحيحًا */
    power_debug_checkpoint(CP_RSDP_CHECKSUM_OK);

    sdt_header_t *fadt_hdr = NULL;

    if (rsdp->Revision >= 2 && rsdp->XsdtAddress)
        fadt_hdr = find_table_xsdt(rsdp->XsdtAddress, "FACP");

    if (!fadt_hdr && rsdp->RsdtAddress)
        fadt_hdr = find_table_rsdt(rsdp->RsdtAddress, "FACP");

    if (!fadt_hdr)
        return; /* لو توقفت هنا: RSDP سليم لكن لم نجد FADT (RSDT/XSDT خطأ؟) */
    power_debug_checkpoint(CP_FADT_FOUND);

    fadt_t *fadt = (fadt_t *)fadt_hdr;

    g_pm1a_cnt_port = (uint16_t)fadt->Pm1aControlBlock;
    g_pm1b_cnt_port = (uint16_t)fadt->Pm1bControlBlock;

    uint64_t dsdt_phys = (fadt->h.Length >= sizeof(fadt_t) && fadt->XDsdt)
                              ? fadt->XDsdt
                              : fadt->Dsdt;

    if (dsdt_phys) {
        power_ensure_mapped(dsdt_phys, sizeof(sdt_header_t));
        sdt_header_t *dsdt = (sdt_header_t *)PHYS_TO_VIRT(dsdt_phys);

        if (mem_eq(dsdt->Signature, "DSDT", 4)) {
            power_ensure_mapped(dsdt_phys, dsdt->Length);
            power_debug_checkpoint(CP_DSDT_MAPPED);

            if (parse_s5(dsdt)) {
                g_acpi_ready = 1;
                power_debug_checkpoint(CP_S5_PARSED_OK);
            }
            /* لو لم تظهر CP_S5_PARSED_OK: DSDT مُخطَّط وصحيح، لكن حيلة
               البحث عن ‎_S5_‎ لم تنجح مع ترميز AML لهذا الجهاز تحديدًا */
        }
    }

    /* منفذ/قيمة إعادة التشغيل عبر ACPI Reset Register إن كانت مدعومة */
    if (fadt->h.Length > offsetof(fadt_t, ResetReg) &&
        (fadt->Flags & FADT_FLAG_RESET_REG_SUPPORTED) &&
        fadt->ResetReg.AddressSpace == 1 /* System I/O */ &&
        fadt->ResetReg.Address != 0) {
        g_reset_port  = (uint16_t)fadt->ResetReg.Address;
        g_reset_value = fadt->ResetValue;
        g_reset_ready = 1;
        power_debug_checkpoint(CP_RESET_REG_CAPTURED);
    }

    power_debug_checkpoint(CP_INIT_DONE);
}

/* ---------------------------------------------------------------------
 * الإغلاق (Shutdown)
 * ------------------------------------------------------------------- */

void system_shutdown(void)
{
    power_debug_checkpoint(CP_SHUTDOWN_ENTER);

    if (g_acpi_ready) {
        power_debug_checkpoint(CP_SHUTDOWN_ACPI_WRITE);

        outw(g_pm1a_cnt_port, (uint16_t)(g_slp_typa | SLP_EN));
        if (g_pm1b_cnt_port)
            outw(g_pm1b_cnt_port, (uint16_t)(g_slp_typb | SLP_EN));

        for (volatile int i = 0; i < 1000000; i++)
            asm volatile("nop");

        /* لو وصلنا هنا، الجهاز لم يُطفأ فعليًا رغم الكتابة الصحيحة -
           غالبًا SLP_TYPa/b غير صحيحة (فشل تحليل ‎_S5_‎) أو المنفذ خطأ */
        power_debug_checkpoint(CP_SHUTDOWN_STILL_ALIVE_AFTER_ACPI);
    }

    /* احتياط/توافق مع بيئات اختبار QEMU القديمة إن لم يتوفر ACPI بعد */
    power_debug_checkpoint(CP_SHUTDOWN_FALLBACK_QEMU_PORTS);
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);

    power_debug_checkpoint(CP_SHUTDOWN_FINAL_HALT);
    while (1)
        asm volatile("hlt");
}

/* ---------------------------------------------------------------------
 * إعادة التشغيل (Reboot)
 *
 * نجرب عدة طرق بالترتيب من الأكثر موثوقية على الأجهزة الحديثة إلى
 * الأقدم، بدل الاعتماد على طريقة وحيدة قد لا تعمل على كل الأجهزة.
 * ------------------------------------------------------------------- */

static void reboot_via_8042(void)
{
    /* مهلة زمنية بدل حلقة انتظار لا نهائية: بعض الأجهزة (خصوصًا التي
       تعتمد كليًا على USB بدون محاكاة PS/2) لا تُصفّر هذا البت أبدًا،
       فتتجمّد النواة هنا للأبد بدون هذا الحد. */
    for (int timeout = 0; timeout < 100000; timeout++) {
        if (!(inb(0x64) & 0x02))
            break;
        power_io_wait();
    }

    outb(0x64, 0xFE);

    for (volatile int i = 0; i < 100000; i++)
        power_io_wait();
}

static void reboot_via_cf9(void)
{
    outb(0xCF9, 0x02);
    power_io_wait();
    outb(0xCF9, 0x06);
    power_io_wait();
    outb(0xCF9, 0x0E);
}

static void reboot_via_triple_fault(void)
{
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idtr = { 0, 0 };

    asm volatile("lidt %0" : : "m"(idtr));
    asm volatile("int $0x03");
}

void system_reboot(void)
{
    if (g_reset_ready)
        outb(g_reset_port, g_reset_value);

    for (volatile int i = 0; i < 100000; i++)
        power_io_wait();

    reboot_via_8042();
    reboot_via_cf9();
    reboot_via_triple_fault();

    /* إن وصلنا هنا فكل الطرق فشلت (سيناريو نادر جدًا) */
    while (1)
        asm volatile("hlt");
}
