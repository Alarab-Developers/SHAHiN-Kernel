#include "مترجم_اللغة.h"
#include "مخزن_المتغيرات.h"
#include "المحلل.h"
#include "مجمع_الترجمة.h"
#include "../ادوات/ادوات.h"
#include "../مفاهيم_اللغة/الدوال.h"
#include "../مفاهيم_اللغة/الحلقات.h"
#include "../مفاهيم_اللغة/الشروط.h"
#include "../مفاهيم_اللغة/رياضيات.h"
#include "../مفاهيم_اللغة/المتغيرات.h"
#include "../مفاهيم_اللغة/الهياكل.h"
#include "../مفاهيم_اللغة/مكتبات_اللغة.h"
#include "التصريح_بالأنواع.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "ادوات/شاشة_التطوير/شاشة_التطوير.h"
#include "../طبقة_التنفيذ/بوابة_التنفيذ.h"
#include "القلب/الوحدات/محمل_التطبيقات/سياق_اللغة.h"
#include "القلب/الوحدات/محمل_التطبيقات/قيم_الملف.h"

/* =========================================================
 * الثوابت
 * ========================================================= */
#define JIT_FRAME_BYTES       512
#define JIT_MAX_DEPTH          16
#define JIT_MAX_ARGC            8
#define TYPED_SLOT_SIZE        24

#define JIT_OP_EQ    0
#define JIT_OP_NEQ   1
#define JIT_OP_LT    2
#define JIT_OP_LTE   3
#define JIT_OP_GT    4
#define JIT_OP_GTE   5

#define JIT_PRINT_BUF_SIZE    512
#define DEFAULT_PRINT_COLOR   0xFF202020u

#define MC_MAX_LOOP_DEPTH      16

/* خانة معامل نوعية — تطابق تخطيط jit_emit_store_typed_slot تمامًا:
 *   +0  : xmm0 (numeric)
 *   +8  : rcx  (text)
 *   +16 : edx  (type)
 */
typedef struct {
    double   numeric;
    char    *text;
    uint32_t type;
    uint32_t pad;
} jit_arg_slot_t;

_Static_assert(sizeof(jit_arg_slot_t) == TYPED_SLOT_SIZE,
               "jit_arg_slot_t must match TYPED_SLOT_SIZE");

/* =========================================================
 * الأنواع الداخلية
 * ========================================================= */

typedef struct {
    size_t  disp_offset;
    char   *name;
    size_t  name_len;
} jit_name_ref_t;

typedef struct {
    uint8_t        *bytes;
    size_t          size;
    size_t          capacity;
    jit_name_ref_t *refs;
    int             ref_count;
    int             ref_capacity;

    jit_reloc_t    *relocs;
    uint32_t        reloc_count;
    uint32_t        reloc_capacity;

    int             aot;
} jit_buf_t;

typedef struct {
    size_t disp_offset;
} mc_reloc_t;

typedef struct {
    size_t     loop_start;
    mc_reloc_t breaks[32];
    int        break_count;
    mc_reloc_t continues[32];
    int        continue_count;
} mc_loop_t;

typedef struct {
    mc_reloc_t *items;
    int         count;
    int         capacity;
} mc_patch_list_t;


/* =========================================================
 * جداول عالمية لنقل قيم المصفوفة من JIT إلى وقت التشغيل
 * ========================================================= */
#define JIT_ARRAY_MAX_VALUES 512

static arp_value_t g_array_values[JIT_ARRAY_MAX_VALUES];
static int         g_array_value_count = 0;
static int         g_array_dims[8];
static int         g_array_ndims = 0;
static arp_type_t  g_array_type  = ARP_TYPE_UNKNOWN;

/* =========================================================
 * تصريحات مسبقة
 * ========================================================= */

static int  jit_compile_expr_typed(const ast_node_t *node, jit_buf_t *b, int *depth);
static int  mc_compile_block     (const char **lines, int line_count, jit_buf_t *b);
static int  jit_helper_symbol_id (const void *fn);
static int  jit_reloc_add        (jit_buf_t *b, size_t offset, uint32_t symbol);

/* =========================================================
 * الحالة العامة
 * ========================================================= */

static char     g_print_buffer[JIT_PRINT_BUF_SIZE];
static int      g_print_pos   = 0;
static uint32_t g_print_color = DEFAULT_PRINT_COLOR;

static const char *const g_arabic_digits[10] =
{
    "٠", "١", "٢", "٣", "٤", "٥", "٦", "٧", "٨", "٩"
};

static mc_patch_list_t g_mc_return_patches;
static mc_loop_t *g_mc_loop_stack[MC_MAX_LOOP_DEPTH];
static int        g_mc_loop_depth = 0;

static jit_result_t g_jit_typed_trace      = { ARP_TYPE_UNKNOWN, 0.0, NULL };
static jit_result_t g_jit_typed_trace_left = { ARP_TYPE_UNKNOWN, 0.0, NULL };

/* =========================================================
 * إدارة الأثر النوعي
 * ========================================================= */

static void jit_typed_trace_clear(void)
{
    if (g_jit_typed_trace.type == ARP_TYPE_TEXT && g_jit_typed_trace.text)
        memory_api.free(g_jit_typed_trace.text);
    g_jit_typed_trace.type = ARP_TYPE_UNKNOWN;
    g_jit_typed_trace.numeric = 0.0;
    g_jit_typed_trace.text = NULL;
}

static void jit_typed_trace_left_clear(void)
{
    if (g_jit_typed_trace_left.type == ARP_TYPE_TEXT && g_jit_typed_trace_left.text)
        memory_api.free(g_jit_typed_trace_left.text);
    g_jit_typed_trace_left.type = ARP_TYPE_UNKNOWN;
    g_jit_typed_trace_left.numeric = 0.0;
    g_jit_typed_trace_left.text = NULL;
}

static void jit_typed_trace_capture_left(void)
{
    jit_typed_trace_left_clear();
    g_jit_typed_trace_left = g_jit_typed_trace;
    g_jit_typed_trace.type = ARP_TYPE_UNKNOWN;
    g_jit_typed_trace.numeric = 0.0;
    g_jit_typed_trace.text = NULL;
}

/* ينشر نتيجة دالة مدمجة في الأثر النوعي.
 * jit_emit_load_typed_result يقرأ xmm0/edx من g_jit_typed_trace بعد كل
 * استدعاء مساعد؛ بدون هذا تضيع القيمة المُعادة (تصير 0 بنوع UNKNOWN). */
static double jit_trace_return(arp_type_t t, double v)
{
    g_jit_typed_trace.type    = t;
    g_jit_typed_trace.numeric = v;
    g_jit_typed_trace.text    = NULL;
    return v;
}

/* =========================================================
 * مخزن الكود
 * ========================================================= */

static void jit_buf_init(jit_buf_t *b)
{
    if (!b) return;
    b->bytes = NULL;
    b->size = 0;
    b->capacity = 0;
    b->refs = NULL;
    b->ref_count = 0;
    b->ref_capacity = 0;
    b->relocs = NULL;
    b->reloc_count = 0;
    b->reloc_capacity = 0;
    b->aot = 0;
}

static void jit_buf_free(jit_buf_t *b)
{
    if (!b) return;
    if (b->bytes) { memory_api.free(b->bytes); b->bytes = NULL; }
    for (int i = 0; i < b->ref_count; i++)
        if (b->refs[i].name) memory_api.free(b->refs[i].name);
    if (b->refs) { memory_api.free(b->refs); b->refs = NULL; }
    if (b->relocs) { memory_api.free(b->relocs); b->relocs = NULL; }
    b->reloc_count = 0;
    b->reloc_capacity = 0;
    b->size = 0;
    b->capacity = 0;
    b->ref_count = 0;
    b->ref_capacity = 0;
}

static int jit_grow(jit_buf_t *b, size_t extra)
{
    if (b->size + extra <= b->capacity) return 0;
    size_t nc = b->capacity ? b->capacity : 256;
    while (nc < b->size + extra) nc *= 2;
    uint8_t *nb = (uint8_t *)memory_api.alloc(nc);
    if (!nb) return -1;
    if (b->bytes) { memcpy(nb, b->bytes, b->size); memory_api.free(b->bytes); }
    b->bytes = nb;
    b->capacity = nc;
    return 0;
}

static int jit_emit8(jit_buf_t *b, uint8_t v)
{
    if (jit_grow(b, 1) < 0) return -1;
    b->bytes[b->size++] = v;
    return 0;
}

static int jit_emit32(jit_buf_t *b, uint32_t v)
{
    if (jit_grow(b, 4) < 0) return -1;
    b->bytes[b->size++] = (uint8_t)((v >>  0) & 0xFF);
    b->bytes[b->size++] = (uint8_t)((v >>  8) & 0xFF);
    b->bytes[b->size++] = (uint8_t)((v >> 16) & 0xFF);
    b->bytes[b->size++] = (uint8_t)((v >> 24) & 0xFF);
    return 0;
}

static int jit_emit_bytes(jit_buf_t *b, const uint8_t *src, size_t n)
{
    if (jit_grow(b, n) < 0) return -1;
    memcpy(b->bytes + b->size, src, n);
    b->size += n;
    return 0;
}

/* =========================================================
 * تعليمات SSE والمسجّلات
 * ========================================================= */

static int jit_emit_movsd_xmm1_xmm0(jit_buf_t *b)
{
    static const uint8_t c[] = { 0xF2, 0x0F, 0x10, 0xC8 };
    return jit_emit_bytes(b, c, sizeof(c));
}

static int jit_emit_load_slot_xmm0(jit_buf_t *b, int d)
{
    if (d < 0 || d >= JIT_MAX_DEPTH) return -1;
    uint8_t disp = (uint8_t)(int8_t)(-8 * (d + 1));
    uint8_t code[5] = { 0xF2, 0x0F, 0x10, 0x45, disp };
    return jit_emit_bytes(b, code, 5);
}

static int jit_emit_load_slot_xmm1(jit_buf_t *b, int d)
{
    if (d < 0 || d >= JIT_MAX_DEPTH) return -1;
    uint8_t disp = (uint8_t)(int8_t)(-8 * (d + 1));
    uint8_t code[5] = { 0xF2, 0x0F, 0x10, 0x4D, disp };
    return jit_emit_bytes(b, code, 5);
}

static int jit_emit_load_slot_xmm2(jit_buf_t *b, int d)
{
    if (d < 0 || d >= JIT_MAX_DEPTH) return -1;
    uint8_t disp = (uint8_t)(int8_t)(-8 * (d + 1));
    uint8_t code[5] = { 0xF2, 0x0F, 0x10, 0x55, disp };
    return jit_emit_bytes(b, code, 5);
}

static int jit_emit_store_slot(jit_buf_t *b, int d)
{
    if (d < 0 || d >= JIT_MAX_DEPTH) return -1;
    uint8_t disp = (uint8_t)(int8_t)(-8 * (d + 1));
    uint8_t code[5] = { 0xF2, 0x0F, 0x11, 0x45, disp };
    return jit_emit_bytes(b, code, 5);
}

static int jit_emit_store_typed_slot(jit_buf_t *b, int d)
{
    if (d < 0 || d >= JIT_MAX_DEPTH) return -1;
    int disp = -TYPED_SLOT_SIZE * (d + 1);

    if (jit_emit8(b, 0xF2) < 0) return -1;
    if (jit_emit8(b, 0x0F) < 0) return -1;
    if (jit_emit8(b, 0x11) < 0) return -1;
    if (jit_emit8(b, 0x85) < 0) return -1;
    if (jit_emit32(b, (uint32_t)disp) < 0) return -1;

    if (jit_emit8(b, 0x48) < 0) return -1;
    if (jit_emit8(b, 0x89) < 0) return -1;
    if (jit_emit8(b, 0x8D) < 0) return -1;
    if (jit_emit32(b, (uint32_t)(disp + 8)) < 0) return -1;

    if (jit_emit8(b, 0x89) < 0) return -1;
    if (jit_emit8(b, 0x95) < 0) return -1;
    if (jit_emit32(b, (uint32_t)(disp + 16)) < 0) return -1;

    return 0;
}

static int jit_emit_load_typed_slot(jit_buf_t *b, int d)
{
    if (d < 0 || d >= JIT_MAX_DEPTH) return -1;
    int disp = -TYPED_SLOT_SIZE * (d + 1);

    if (jit_emit8(b, 0xF2) < 0) return -1;
    if (jit_emit8(b, 0x0F) < 0) return -1;
    if (jit_emit8(b, 0x10) < 0) return -1;
    if (jit_emit8(b, 0x85) < 0) return -1;
    if (jit_emit32(b, (uint32_t)disp) < 0) return -1;

    if (jit_emit8(b, 0x48) < 0) return -1;
    if (jit_emit8(b, 0x8B) < 0) return -1;
    if (jit_emit8(b, 0x8D) < 0) return -1;
    if (jit_emit32(b, (uint32_t)(disp + 8)) < 0) return -1;

    if (jit_emit8(b, 0x8B) < 0) return -1;
    if (jit_emit8(b, 0x95) < 0) return -1;
    if (jit_emit32(b, (uint32_t)(disp + 16)) < 0) return -1;

    return 0;
}

/* تحميل النتيجة النوعية من g_jit_typed_trace إلى xmm0/edx/rcx
 *
 * layout لـ jit_result_t على x86_64:
 *   +0  : arp_type_t (4 بايت)
 *   +4  : padding
 *   +8  : double numeric
 *   +16 : char* text
 */
static int jit_emit_load_typed_result(jit_buf_t *b)
{
    /* movabs r11, imm64 — عنوان g_jit_typed_trace.
     * في وضع AOT لا يجوز تضمين العنوان المطلق للمضيف داخل الملف،
     * لذلك يُسجَّل كترحيل (رمز في g_jit_symbols) ويكتبه المحمّل لاحقًا. */
    if (jit_emit8(b, 0x49) < 0) return -1;
    if (jit_emit8(b, 0xBB) < 0) return -1;

    if (b->aot)
    {
        int id = jit_helper_symbol_id((const void *)&g_jit_typed_trace);
        if (id < 0) return -1;
        if (jit_reloc_add(b, b->size, (uint32_t)id) < 0) return -1;
        for (int i = 0; i < 8; i++)
            if (jit_emit8(b, 0) < 0) return -1;
    }
    else
    {
        uint64_t addr = (uint64_t)(uintptr_t)&g_jit_typed_trace;
        for (int i = 0; i < 8; i++)
            if (jit_emit8(b, (uint8_t)((addr >> (i * 8)) & 0xFF)) < 0) return -1;
    }

    { static const uint8_t c[] = { 0x41, 0x8B, 0x13 };
      if (jit_emit_bytes(b, c, sizeof(c)) < 0) return -1; }

    { static const uint8_t c[] = { 0xF2, 0x41, 0x0F, 0x10, 0x43, 0x08 };
      if (jit_emit_bytes(b, c, sizeof(c)) < 0) return -1; }

    { static const uint8_t c[] = { 0x49, 0x8B, 0x4B, 0x10 };
      if (jit_emit_bytes(b, c, sizeof(c)) < 0) return -1; }

    return 0;
}

/* =========================================================
 * تعليمات x86_64 مساعدة
 * ========================================================= */

static int jit_emit_mov_edi_imm(jit_buf_t *b, uint32_t v)
{
    if (jit_emit8(b, 0xBF) < 0) return -1;
    return jit_emit32(b, v);
}

static int jit_emit_mov_esi_imm(jit_buf_t *b, uint32_t v)
{
    if (jit_emit8(b, 0xBE) < 0) return -1;
    return jit_emit32(b, v);
}

static int jit_emit_mov_edx_imm(jit_buf_t *b, uint32_t v)
{
    if (jit_emit8(b, 0xBA) < 0) return -1;
    return jit_emit32(b, v);
}

static int jit_emit_load_double(jit_buf_t *b, double d)
{
    uint64_t bits;
    memcpy(&bits, &d, 8);
    static const uint8_t head[] = { 0x48, 0xB8 };
    static const uint8_t tail[] = { 0x66, 0x48, 0x0F, 0x6E, 0xC0 };
    if (jit_emit_bytes(b, head, sizeof(head)) < 0) return -1;
    for (int i = 0; i < 8; i++)
        if (jit_emit8(b, (uint8_t)((bits >> (i * 8)) & 0xFF)) < 0) return -1;
    return jit_emit_bytes(b, tail, sizeof(tail));
}

static int jit_emit_xor_rcx_rcx(jit_buf_t *b)
{
    static const uint8_t c[] = { 0x48, 0x31, 0xC9 };
    return jit_emit_bytes(b, c, sizeof(c));
}

static int jit_emit_mov_rcx_rdi(jit_buf_t *b)
{
    static const uint8_t c[] = { 0x48, 0x89, 0xF9 };
    return jit_emit_bytes(b, c, sizeof(c));
}

static int jit_emit_xorpd_xmm0_xmm0(jit_buf_t *b)
{
    static const uint8_t c[] = { 0x66, 0x0F, 0x57, 0xC0 };
    return jit_emit_bytes(b, c, sizeof(c));
}

__attribute__((unused))
static int jit_emit_lea_rcx_rbp_neg(jit_buf_t *b, uint32_t disp)
{
    if (disp <= 127) {
        uint8_t code[4] = { 0x48, 0x8D, 0x4D, (uint8_t)(int8_t)(-(int32_t)disp) };
        return jit_emit_bytes(b, code, 4);
    }
    if (jit_emit8(b, 0x48) < 0) return -1;
    if (jit_emit8(b, 0x8D) < 0) return -1;
    if (jit_emit8(b, 0x8D) < 0) return -1;
    return jit_emit32(b, (uint32_t)(-(int32_t)disp));
}

static int jit_emit_lea_rdx_rbp_neg(jit_buf_t *b, uint32_t disp)
{
    if (disp <= 127) {
        uint8_t code[4] = { 0x48, 0x8D, 0x55, (uint8_t)(int8_t)(-(int32_t)disp) };
        return jit_emit_bytes(b, code, 4);
    }
    if (jit_emit8(b, 0x48) < 0) return -1;
    if (jit_emit8(b, 0x8D) < 0) return -1;
    if (jit_emit8(b, 0x95) < 0) return -1;
    return jit_emit32(b, (uint32_t)(-(int32_t)disp));
}

static int jit_emit_cvttsd2si_rax_xmm0(jit_buf_t *b)
{
    static const uint8_t c[] = { 0xF2, 0x48, 0x0F, 0x2C, 0xC0 };
    return jit_emit_bytes(b, c, sizeof(c));
}

/* =========================================================
 * الترحيلات
 * ========================================================= */

static int jit_reloc_add(jit_buf_t *b, size_t offset, uint32_t symbol)
{
    if (b->reloc_count >= b->reloc_capacity)
    {
        uint32_t nc = b->reloc_capacity ? b->reloc_capacity * 2 : 64;
        jit_reloc_t *nr = (jit_reloc_t *)memory_api.alloc(nc * sizeof(jit_reloc_t));
        if (!nr) return -1;
        for (uint32_t i = 0; i < b->reloc_count; i++) nr[i] = b->relocs[i];
        if (b->relocs) memory_api.free(b->relocs);
        b->relocs = nr;
        b->reloc_capacity = nc;
    }
    b->relocs[b->reloc_count].offset = (uint32_t)offset;
    b->relocs[b->reloc_count].symbol = symbol;
    b->reloc_count++;
    return 0;
}

static int jit_emit_call_helper(jit_buf_t *b, const void *fn)
{
    uint64_t addr = (uint64_t)(uintptr_t)fn;

    if (b->aot)
    {
        int id = jit_helper_symbol_id(fn);
        if (id < 0) return -1;

        if (jit_emit8(b, 0x48) < 0) return -1;
        if (jit_emit8(b, 0xB8) < 0) return -1;
        if (jit_reloc_add(b, b->size, (uint32_t)id) < 0) return -1;
        for (int i = 0; i < 8; i++)
            if (jit_emit8(b, 0) < 0) return -1;
        if (jit_emit8(b, 0xFF) < 0) return -1;
        if (jit_emit8(b, 0xD0) < 0) return -1;
        return 0;
    }

    if (jit_emit8(b, 0x48) < 0) return -1;
    if (jit_emit8(b, 0xB8) < 0) return -1;
    for (int i = 0; i < 8; i++)
        if (jit_emit8(b, (uint8_t)((addr >> (i * 8)) & 0xFF)) < 0) return -1;
    if (jit_emit8(b, 0xFF) < 0) return -1;
    if (jit_emit8(b, 0xD0) < 0) return -1;
    return 0;
}

/* =========================================================
 * مراجع الأسماء (RIP-relative)
 * ========================================================= */

static int jit_emit_name_ref_reg(jit_buf_t *b, const char *name, uint8_t modrm)
{
    if (!name) return -1;

    if (jit_emit8(b, 0x48) < 0) return -1;
    if (jit_emit8(b, 0x8D) < 0) return -1;
    if (jit_emit8(b, modrm) < 0) return -1;

    size_t disp_off = b->size;
    if (jit_emit32(b, 0) < 0) return -1;

    if (b->ref_count >= b->ref_capacity)
    {
        int nc = b->ref_capacity ? b->ref_capacity * 2 : 8;
        jit_name_ref_t *nr = (jit_name_ref_t *)memory_api.alloc(nc * sizeof(jit_name_ref_t));
        if (!nr) return -1;
        for (int i = 0; i < b->ref_count; i++) nr[i] = b->refs[i];
        if (b->refs) memory_api.free(b->refs);
        b->refs = nr;
        b->ref_capacity = nc;
    }

    size_t nlen = strlen(name);
    char *copy = (char *)memory_api.alloc(nlen + 1);
    if (!copy) return -1;
    memcpy(copy, name, nlen + 1);

    b->refs[b->ref_count].disp_offset = disp_off;
    b->refs[b->ref_count].name        = copy;
    b->refs[b->ref_count].name_len    = nlen;
    b->ref_count++;
    return 0;
}

static int jit_emit_name_ref    (jit_buf_t *b, const char *name) { return jit_emit_name_ref_reg(b, name, 0x3D); }
static int jit_emit_name_ref_rsi(jit_buf_t *b, const char *name) { return jit_emit_name_ref_reg(b, name, 0x35); }

/* =========================================================
 * ★★★ دوال وقت التشغيل النوعية ★★★
 * ========================================================= */

__attribute__((used, noinline))
void jit_rt_typed_get_variable(const char *name)
{
    jit_typed_trace_clear();
    if (!name) return;

    arp_value_t v;
    arp_value_init(&v, ARP_TYPE_UNKNOWN);
    if (variable_get_value(name, &v) < 0) return;

    switch (v.type) {
        case ARP_TYPE_TEXT:
            g_jit_typed_trace.type = ARP_TYPE_TEXT;
            g_jit_typed_trace.text = v.as.text;
            v.as.text = NULL;
            break;
        case ARP_TYPE_INT:    g_jit_typed_trace.type = ARP_TYPE_INT;    g_jit_typed_trace.numeric = (double)v.as.i64; break;
        case ARP_TYPE_FLOAT:  g_jit_typed_trace.type = ARP_TYPE_FLOAT;  g_jit_typed_trace.numeric = (double)v.as.f32; break;
        case ARP_TYPE_DOUBLE: g_jit_typed_trace.type = ARP_TYPE_DOUBLE; g_jit_typed_trace.numeric = v.as.f64; break;
        case ARP_TYPE_CHAR:   g_jit_typed_trace.type = ARP_TYPE_CHAR;   g_jit_typed_trace.numeric = (double)v.as.cp; break;
        case ARP_TYPE_BOOL:   g_jit_typed_trace.type = ARP_TYPE_BOOL;   g_jit_typed_trace.numeric = (double)v.as.b; break;
        case ARP_TYPE_PTR:    g_jit_typed_trace.type = ARP_TYPE_PTR;    g_jit_typed_trace.numeric = (double)(uint64_t)v.as.ptr; break;
        default: break;
    }
    arp_value_free(&v);
}

__attribute__((used, noinline))
void jit_rt_typed_set_variable(const char *name)
{
    if (!name) { jit_typed_trace_clear(); return; }

    arp_value_t v;
    arp_value_init(&v, g_jit_typed_trace.type);
    switch (g_jit_typed_trace.type) {
        case ARP_TYPE_TEXT:
            if (arp_value_make_text(&v, g_jit_typed_trace.text ? g_jit_typed_trace.text : "", -1) < 0) return;
            break;
        case ARP_TYPE_INT:    arp_value_make_int(&v, (int64_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_FLOAT:  arp_value_make_float(&v, (float)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_DOUBLE: arp_value_make_double(&v, g_jit_typed_trace.numeric); break;
        case ARP_TYPE_CHAR:   arp_value_make_char(&v, (uint32_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_BOOL:   arp_value_make_bool(&v, (int)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_PTR:    arp_value_make_ptr(&v, (uintptr_t)(uint64_t)g_jit_typed_trace.numeric); break;
        default: break;
    }
    variable_set_value(name, &v);
    arp_value_free(&v);
}

__attribute__((used, noinline))
void jit_rt_typed_declare_variable(const char *name, int type)
{
    if (!name) { jit_typed_trace_clear(); return; }

    arp_value_t v;
    arp_value_init(&v, (arp_type_t)type);

    arp_value_t src;
    arp_value_init(&src, g_jit_typed_trace.type);
    switch (g_jit_typed_trace.type) {
        case ARP_TYPE_TEXT:
            if (arp_value_make_text(&src, g_jit_typed_trace.text ? g_jit_typed_trace.text : "", -1) < 0) return;
            break;
        case ARP_TYPE_INT:    arp_value_make_int(&src, (int64_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_FLOAT:  arp_value_make_float(&src, (float)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_DOUBLE: arp_value_make_double(&src, g_jit_typed_trace.numeric); break;
        case ARP_TYPE_CHAR:   arp_value_make_char(&src, (uint32_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_BOOL:   arp_value_make_bool(&src, (int)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_PTR:    arp_value_make_ptr(&src, (uintptr_t)(uint64_t)g_jit_typed_trace.numeric); break;
        default: break;
    }

    if (!arp_value_is_assignable((arp_type_t)type, src.type)) {
        arp_value_free(&src);
        jit_typed_trace_clear();
        return;
    }
    if (arp_value_coerce(&src, &v) < 0) {
        arp_value_free(&src);
        jit_typed_trace_clear();
        return;
    }
    arp_value_free(&src);

    variable_declare(name, (arp_type_t)type, &v);

    jit_typed_trace_clear();
    switch (v.type) {
        case ARP_TYPE_TEXT:
            g_jit_typed_trace.type = ARP_TYPE_TEXT;
            g_jit_typed_trace.text = v.as.text;
            v.as.text = NULL;
            break;
        case ARP_TYPE_INT:    g_jit_typed_trace.type = ARP_TYPE_INT;    g_jit_typed_trace.numeric = (double)v.as.i64; break;
        case ARP_TYPE_FLOAT:  g_jit_typed_trace.type = ARP_TYPE_FLOAT;  g_jit_typed_trace.numeric = (double)v.as.f32; break;
        case ARP_TYPE_DOUBLE: g_jit_typed_trace.type = ARP_TYPE_DOUBLE; g_jit_typed_trace.numeric = v.as.f64; break;
        case ARP_TYPE_CHAR:   g_jit_typed_trace.type = ARP_TYPE_CHAR;   g_jit_typed_trace.numeric = (double)v.as.cp; break;
        case ARP_TYPE_BOOL:   g_jit_typed_trace.type = ARP_TYPE_BOOL;   g_jit_typed_trace.numeric = (double)v.as.b; break;
        case ARP_TYPE_PTR:    g_jit_typed_trace.type = ARP_TYPE_PTR;    g_jit_typed_trace.numeric = (double)(uint64_t)v.as.ptr; break;
        default: break;
    }
    arp_value_free(&v);
}

/* ★ بناء مصفوفة من g_array_values/g_array_dims/g_array_type ثم تسجيلها
 *   في متغيرات اللغة. تُستدعى من JIT بعد تعبئة الجداول. */
__attribute__((used, noinline))
void jit_rt_declare_array(const char *name)
{
    if (!name || g_array_ndims <= 0 || g_array_value_count <= 0)
    {
        jit_typed_trace_clear();
        return;
    }

    int total = 1;
    for (int i = 0; i < g_array_ndims; i++) total *= g_array_dims[i];

    if (total != g_array_value_count)
    {
        jit_typed_trace_clear();
        return;
    }

    /* إنشاء المصفوفة عبر الواجهة العامة فقط */
    arp_array_t *arr = arp_array_create(g_array_type, g_array_ndims);
    if (!arr) { jit_typed_trace_clear(); return; }

    if (arp_array_init(arr, g_array_ndims, g_array_dims,
                       g_array_values, total) < 0)
    {
        arp_array_free(arr);
        jit_typed_trace_clear();
        return;
    }

    arp_value_t v;
    arp_value_init(&v, ARP_TYPE_ARRAY);
    v.as.array = arr;
    variable_declare(name, ARP_TYPE_ARRAY, &v);

    jit_typed_trace_clear();
    g_jit_typed_trace.type = ARP_TYPE_ARRAY;
    g_jit_typed_trace.numeric = 0.0;
    g_jit_typed_trace.text = NULL;

    arp_value_free(&v);
}

__attribute__((used, noinline))
void jit_rt_typed_trace_from_regs(void)
{
    uint32_t t;
    uint64_t txt;
    double  num;
    __asm__ volatile (
        "mov %%edx, %0\n"
        "mov %%rcx, %1\n"
        "movq %%xmm0, %2\n"
        : "=m"(t), "=m"(txt), "=m"(num)
        :
        : "memory"
    );
    jit_typed_trace_clear();
    g_jit_typed_trace.type = (arp_type_t)t;
    g_jit_typed_trace.numeric = num;
    g_jit_typed_trace.text = (char *)(uintptr_t)txt;
}

/* ★ قيمة ارجع(...): تنقل السجلات (xmm0/edx/rcx) إلى الأثر العام.
 *   machine_jit_execute_typed يقرأ نتيجة الدالة من g_jit_typed_trace؛ وارجع(4)
 *   مثلًا تضع القيمة في السجلات فقط، فكان المُستدعي يستلم أثرًا قديمًا
 *   (آخر نتيجة مقارنة) بدل 4. النص يُنسخ لأن rcx قد يشير إلى حرفية داخل
 *   الكود (لا تُحرَّر) أو إلى نص يملكه الأثر نفسه (يُحرَّر عند المسح). */
__attribute__((used, noinline))
void jit_rt_typed_return_from_regs(void)
{
    uint32_t t;
    uint64_t txt;
    double  num;
    __asm__ volatile (
        "mov %%edx, %0\n"
        "mov %%rcx, %1\n"
        "movq %%xmm0, %2\n"
        : "=m"(t), "=m"(txt), "=m"(num)
        :
        : "memory"
    );

    char *copy = NULL;
    if ((arp_type_t)t == ARP_TYPE_TEXT)
        copy = arp_strdup(txt ? (const char *)(uintptr_t)txt : "");

    jit_typed_trace_clear();
    g_jit_typed_trace.type    = (arp_type_t)t;
    g_jit_typed_trace.numeric = num;
    g_jit_typed_trace.text    = copy;
}

__attribute__((used, noinline))
void jit_rt_typed_capture_left(void)
{
    jit_typed_trace_capture_left();
}

__attribute__((used, noinline))
void jit_rt_typed_add(void)
{
    arp_value_t a, b, out;
    arp_value_init(&a, ARP_TYPE_UNKNOWN);
    arp_value_init(&b, ARP_TYPE_UNKNOWN);
    arp_value_init(&out, ARP_TYPE_UNKNOWN);

    switch (g_jit_typed_trace_left.type) {
        case ARP_TYPE_TEXT: if (arp_value_make_text(&a, g_jit_typed_trace_left.text ? g_jit_typed_trace_left.text : "", -1) < 0) goto done; break;
        case ARP_TYPE_INT:    arp_value_make_int(&a, (int64_t)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_FLOAT:  arp_value_make_float(&a, (float)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_DOUBLE: arp_value_make_double(&a, g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_CHAR:   arp_value_make_char(&a, (uint32_t)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_BOOL:   arp_value_make_bool(&a, (int)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_PTR:    arp_value_make_ptr(&a, (uintptr_t)(uint64_t)g_jit_typed_trace_left.numeric); break;
        default: goto done;
    }
    switch (g_jit_typed_trace.type) {
        case ARP_TYPE_TEXT: if (arp_value_make_text(&b, g_jit_typed_trace.text ? g_jit_typed_trace.text : "", -1) < 0) goto done; break;
        case ARP_TYPE_INT:    arp_value_make_int(&b, (int64_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_FLOAT:  arp_value_make_float(&b, (float)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_DOUBLE: arp_value_make_double(&b, g_jit_typed_trace.numeric); break;
        case ARP_TYPE_CHAR:   arp_value_make_char(&b, (uint32_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_BOOL:   arp_value_make_bool(&b, (int)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_PTR:    arp_value_make_ptr(&b, (uintptr_t)(uint64_t)g_jit_typed_trace.numeric); break;
        default: goto done;
    }
    if (arp_value_add(&a, &b, &out) == 0) {
        jit_typed_trace_clear();
        switch (out.type) {
            case ARP_TYPE_TEXT: g_jit_typed_trace.type = ARP_TYPE_TEXT; g_jit_typed_trace.text = out.as.text; out.as.text = NULL; break;
            case ARP_TYPE_INT:    g_jit_typed_trace.type = ARP_TYPE_INT;    g_jit_typed_trace.numeric = (double)out.as.i64; break;
            case ARP_TYPE_FLOAT:  g_jit_typed_trace.type = ARP_TYPE_FLOAT;  g_jit_typed_trace.numeric = (double)out.as.f32; break;
            case ARP_TYPE_DOUBLE: g_jit_typed_trace.type = ARP_TYPE_DOUBLE; g_jit_typed_trace.numeric = out.as.f64; break;
            case ARP_TYPE_CHAR:   g_jit_typed_trace.type = ARP_TYPE_CHAR;   g_jit_typed_trace.numeric = (double)out.as.cp; break;
            case ARP_TYPE_BOOL:   g_jit_typed_trace.type = ARP_TYPE_BOOL;   g_jit_typed_trace.numeric = (double)out.as.b; break;
            case ARP_TYPE_PTR:    g_jit_typed_trace.type = ARP_TYPE_PTR;    g_jit_typed_trace.numeric = (double)(uint64_t)out.as.ptr; break;
            default: break;
        }
    } else {
        jit_typed_trace_clear();   /* فشل الجمع → UNKNOWN بدل بقاء قيمة الطرف الأيمن */
    }
done:
    arp_value_free(&a);
    arp_value_free(&b);
    arp_value_free(&out);
    jit_typed_trace_left_clear();
}

#define JIT_TYPED_ARITH_DEFINE(NAME, FUNC) \
__attribute__((used, noinline)) \
void jit_rt_typed_##NAME(void) \
{ \
    arp_value_t a, b, out; \
    arp_value_init(&a, ARP_TYPE_UNKNOWN); \
    arp_value_init(&b, ARP_TYPE_UNKNOWN); \
    arp_value_init(&out, ARP_TYPE_UNKNOWN); \
    switch (g_jit_typed_trace_left.type) { \
        case ARP_TYPE_TEXT: if (arp_value_make_text(&a, g_jit_typed_trace_left.text ? g_jit_typed_trace_left.text : "", -1) < 0) goto done; break; \
        case ARP_TYPE_INT:    arp_value_make_int(&a, (int64_t)g_jit_typed_trace_left.numeric); break; \
        case ARP_TYPE_FLOAT:  arp_value_make_float(&a, (float)g_jit_typed_trace_left.numeric); break; \
        case ARP_TYPE_DOUBLE: arp_value_make_double(&a, g_jit_typed_trace_left.numeric); break; \
        case ARP_TYPE_CHAR:   arp_value_make_char(&a, (uint32_t)g_jit_typed_trace_left.numeric); break; \
        case ARP_TYPE_BOOL:   arp_value_make_bool(&a, (int)g_jit_typed_trace_left.numeric); break; \
        case ARP_TYPE_PTR:    arp_value_make_ptr(&a, (uintptr_t)(uint64_t)g_jit_typed_trace_left.numeric); break; \
        default: goto done; \
    } \
    switch (g_jit_typed_trace.type) { \
        case ARP_TYPE_TEXT: if (arp_value_make_text(&b, g_jit_typed_trace.text ? g_jit_typed_trace.text : "", -1) < 0) goto done; break; \
        case ARP_TYPE_INT:    arp_value_make_int(&b, (int64_t)g_jit_typed_trace.numeric); break; \
        case ARP_TYPE_FLOAT:  arp_value_make_float(&b, (float)g_jit_typed_trace.numeric); break; \
        case ARP_TYPE_DOUBLE: arp_value_make_double(&b, g_jit_typed_trace.numeric); break; \
        case ARP_TYPE_CHAR:   arp_value_make_char(&b, (uint32_t)g_jit_typed_trace.numeric); break; \
        case ARP_TYPE_BOOL:   arp_value_make_bool(&b, (int)g_jit_typed_trace.numeric); break; \
        case ARP_TYPE_PTR:    arp_value_make_ptr(&b, (uintptr_t)(uint64_t)g_jit_typed_trace.numeric); break; \
        default: goto done; \
    } \
    if (FUNC(&a, &b, &out) == 0) { \
        jit_typed_trace_clear(); \
        switch (out.type) { \
            case ARP_TYPE_TEXT: g_jit_typed_trace.type = ARP_TYPE_TEXT; g_jit_typed_trace.text = out.as.text; out.as.text = NULL; break; \
            case ARP_TYPE_INT:    g_jit_typed_trace.type = ARP_TYPE_INT;    g_jit_typed_trace.numeric = (double)out.as.i64; break; \
            case ARP_TYPE_FLOAT:  g_jit_typed_trace.type = ARP_TYPE_FLOAT;  g_jit_typed_trace.numeric = (double)out.as.f32; break; \
            case ARP_TYPE_DOUBLE: g_jit_typed_trace.type = ARP_TYPE_DOUBLE; g_jit_typed_trace.numeric = out.as.f64; break; \
            case ARP_TYPE_CHAR:   g_jit_typed_trace.type = ARP_TYPE_CHAR;   g_jit_typed_trace.numeric = (double)out.as.cp; break; \
            case ARP_TYPE_BOOL:   g_jit_typed_trace.type = ARP_TYPE_BOOL;   g_jit_typed_trace.numeric = (double)out.as.b; break; \
            case ARP_TYPE_PTR:    g_jit_typed_trace.type = ARP_TYPE_PTR;    g_jit_typed_trace.numeric = (double)(uint64_t)out.as.ptr; break; \
            default: break; \
        } \
    } else { \
        jit_typed_trace_clear(); \
    } \
done: \
    arp_value_free(&a); arp_value_free(&b); arp_value_free(&out); \
    jit_typed_trace_left_clear(); \
}

JIT_TYPED_ARITH_DEFINE(sub, arp_value_sub)
JIT_TYPED_ARITH_DEFINE(mul, arp_value_mul)
JIT_TYPED_ARITH_DEFINE(div, arp_value_div)
#undef JIT_TYPED_ARITH_DEFINE

#define JIT_BITOP_DEFINE(NAME, FUNC) \
__attribute__((used, noinline)) \
void jit_rt_typed_##NAME(void) \
{ \
    arp_value_t a, b, out; \
    arp_value_init(&a, ARP_TYPE_UNKNOWN); \
    arp_value_init(&b, ARP_TYPE_UNKNOWN); \
    arp_value_init(&out, ARP_TYPE_UNKNOWN); \
    switch (g_jit_typed_trace_left.type) { \
        case ARP_TYPE_TEXT: if (arp_value_make_text(&a, g_jit_typed_trace_left.text ? g_jit_typed_trace_left.text : "", -1) < 0) goto done; break; \
        case ARP_TYPE_INT:    arp_value_make_int(&a, (int64_t)g_jit_typed_trace_left.numeric); break; \
        case ARP_TYPE_FLOAT:  arp_value_make_float(&a, (float)g_jit_typed_trace_left.numeric); break; \
        case ARP_TYPE_DOUBLE: arp_value_make_double(&a, g_jit_typed_trace_left.numeric); break; \
        case ARP_TYPE_CHAR:   arp_value_make_char(&a, (uint32_t)g_jit_typed_trace_left.numeric); break; \
        case ARP_TYPE_BOOL:   arp_value_make_bool(&a, (int)g_jit_typed_trace_left.numeric); break; \
        case ARP_TYPE_PTR:    arp_value_make_ptr(&a, (uintptr_t)(uint64_t)g_jit_typed_trace_left.numeric); break; \
        default: goto done; \
    } \
    switch (g_jit_typed_trace.type) { \
        case ARP_TYPE_TEXT: if (arp_value_make_text(&b, g_jit_typed_trace.text ? g_jit_typed_trace.text : "", -1) < 0) goto done; break; \
        case ARP_TYPE_INT:    arp_value_make_int(&b, (int64_t)g_jit_typed_trace.numeric); break; \
        case ARP_TYPE_FLOAT:  arp_value_make_float(&b, (float)g_jit_typed_trace.numeric); break; \
        case ARP_TYPE_DOUBLE: arp_value_make_double(&b, g_jit_typed_trace.numeric); break; \
        case ARP_TYPE_CHAR:   arp_value_make_char(&b, (uint32_t)g_jit_typed_trace.numeric); break; \
        case ARP_TYPE_BOOL:   arp_value_make_bool(&b, (int)g_jit_typed_trace.numeric); break; \
        case ARP_TYPE_PTR:    arp_value_make_ptr(&b, (uintptr_t)(uint64_t)g_jit_typed_trace.numeric); break; \
        default: goto done; \
    } \
    if (FUNC(&a, &b, &out) == 0) { \
        jit_typed_trace_clear(); \
        switch (out.type) { \
            case ARP_TYPE_INT:    g_jit_typed_trace.type = ARP_TYPE_INT;    g_jit_typed_trace.numeric = (double)out.as.i64; break; \
            case ARP_TYPE_FLOAT:  g_jit_typed_trace.type = ARP_TYPE_FLOAT;  g_jit_typed_trace.numeric = (double)out.as.f32; break; \
            case ARP_TYPE_DOUBLE: g_jit_typed_trace.type = ARP_TYPE_DOUBLE; g_jit_typed_trace.numeric = out.as.f64; break; \
            default: break; \
        } \
    } else { \
        jit_typed_trace_clear(); \
    } \
done: \
    arp_value_free(&a); arp_value_free(&b); arp_value_free(&out); \
    jit_typed_trace_left_clear(); \
}

JIT_BITOP_DEFINE(mod,     arp_value_mod)
JIT_BITOP_DEFINE(shl,     arp_value_shl)
JIT_BITOP_DEFINE(shr,     arp_value_shr)
JIT_BITOP_DEFINE(bit_and, arp_value_bit_and)
JIT_BITOP_DEFINE(bit_or,  arp_value_bit_or)
JIT_BITOP_DEFINE(bit_xor, arp_value_bit_xor)
#undef JIT_BITOP_DEFINE


__attribute__((used, noinline))
void jit_rt_typed_neg(void)
{
    arp_value_t a, out;
    arp_value_init(&a, ARP_TYPE_UNKNOWN);
    arp_value_init(&out, ARP_TYPE_UNKNOWN);
    switch (g_jit_typed_trace.type) {
        case ARP_TYPE_INT:    arp_value_make_int(&a, (int64_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_FLOAT:  arp_value_make_float(&a, (float)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_DOUBLE: arp_value_make_double(&a, g_jit_typed_trace.numeric); break;
        case ARP_TYPE_CHAR:   arp_value_make_char(&a, (uint32_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_BOOL:   arp_value_make_bool(&a, (int)g_jit_typed_trace.numeric); break;
        default: goto done;
    }
    if (arp_value_neg(&a, &out) == 0) {
        jit_typed_trace_clear();
        switch (out.type) {
            case ARP_TYPE_INT:    g_jit_typed_trace.type = ARP_TYPE_INT;    g_jit_typed_trace.numeric = (double)out.as.i64; break;
            case ARP_TYPE_FLOAT:  g_jit_typed_trace.type = ARP_TYPE_FLOAT;  g_jit_typed_trace.numeric = (double)out.as.f32; break;
            case ARP_TYPE_DOUBLE: g_jit_typed_trace.type = ARP_TYPE_DOUBLE; g_jit_typed_trace.numeric = out.as.f64; break;
            default: break;
        }
    }
done:
    arp_value_free(&a);
    arp_value_free(&out);
}

/* منطقي AND — الطرفان في g_jit_typed_trace_left / g_jit_typed_trace */
__attribute__((used, noinline))
void jit_rt_typed_and(void)
{
    arp_value_t a, b;
    arp_value_init(&a, ARP_TYPE_UNKNOWN);
    arp_value_init(&b, ARP_TYPE_UNKNOWN);

    int ok = 0;

    switch (g_jit_typed_trace_left.type) {
        case ARP_TYPE_TEXT: if (arp_value_make_text(&a, g_jit_typed_trace_left.text ? g_jit_typed_trace_left.text : "", -1) < 0) goto done; break;
        case ARP_TYPE_INT:    arp_value_make_int(&a, (int64_t)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_FLOAT:  arp_value_make_float(&a, (float)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_DOUBLE: arp_value_make_double(&a, g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_CHAR:   arp_value_make_char(&a, (uint32_t)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_BOOL:   arp_value_make_bool(&a, (int)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_PTR:    arp_value_make_ptr(&a, (uintptr_t)(uint64_t)g_jit_typed_trace_left.numeric); break;
        default: goto done;
    }
    switch (g_jit_typed_trace.type) {
        case ARP_TYPE_TEXT: if (arp_value_make_text(&b, g_jit_typed_trace.text ? g_jit_typed_trace.text : "", -1) < 0) goto done; break;
        case ARP_TYPE_INT:    arp_value_make_int(&b, (int64_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_FLOAT:  arp_value_make_float(&b, (float)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_DOUBLE: arp_value_make_double(&b, g_jit_typed_trace.numeric); break;
        case ARP_TYPE_CHAR:   arp_value_make_char(&b, (uint32_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_BOOL:   arp_value_make_bool(&b, (int)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_PTR:    arp_value_make_ptr(&b, (uintptr_t)(uint64_t)g_jit_typed_trace.numeric); break;
        default: goto done;
    }

    ok = (arp_value_truthy(&a) && arp_value_truthy(&b)) ? 1 : 0;

done:
    arp_value_free(&a);
    arp_value_free(&b);
    jit_typed_trace_clear();
    g_jit_typed_trace.type = ARP_TYPE_BOOL;
    g_jit_typed_trace.numeric = (double)ok;
    jit_typed_trace_left_clear();
}

__attribute__((used, noinline))
void jit_rt_typed_or(void)
{
    arp_value_t a, b;
    arp_value_init(&a, ARP_TYPE_UNKNOWN);
    arp_value_init(&b, ARP_TYPE_UNKNOWN);

    int ok = 0;

    switch (g_jit_typed_trace_left.type) {
        case ARP_TYPE_TEXT: if (arp_value_make_text(&a, g_jit_typed_trace_left.text ? g_jit_typed_trace_left.text : "", -1) < 0) goto done; break;
        case ARP_TYPE_INT:    arp_value_make_int(&a, (int64_t)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_FLOAT:  arp_value_make_float(&a, (float)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_DOUBLE: arp_value_make_double(&a, g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_CHAR:   arp_value_make_char(&a, (uint32_t)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_BOOL:   arp_value_make_bool(&a, (int)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_PTR:    arp_value_make_ptr(&a, (uintptr_t)(uint64_t)g_jit_typed_trace_left.numeric); break;
        default: goto done;
    }
    switch (g_jit_typed_trace.type) {
        case ARP_TYPE_TEXT: if (arp_value_make_text(&b, g_jit_typed_trace.text ? g_jit_typed_trace.text : "", -1) < 0) goto done; break;
        case ARP_TYPE_INT:    arp_value_make_int(&b, (int64_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_FLOAT:  arp_value_make_float(&b, (float)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_DOUBLE: arp_value_make_double(&b, g_jit_typed_trace.numeric); break;
        case ARP_TYPE_CHAR:   arp_value_make_char(&b, (uint32_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_BOOL:   arp_value_make_bool(&b, (int)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_PTR:    arp_value_make_ptr(&b, (uintptr_t)(uint64_t)g_jit_typed_trace.numeric); break;
        default: goto done;
    }

    ok = (arp_value_truthy(&a) || arp_value_truthy(&b)) ? 1 : 0;

done:
    arp_value_free(&a);
    arp_value_free(&b);
    jit_typed_trace_clear();
    g_jit_typed_trace.type = ARP_TYPE_BOOL;
    g_jit_typed_trace.numeric = (double)ok;
    jit_typed_trace_left_clear();
}


__attribute__((used, noinline))
void jit_rt_typed_compare(int op)
{
    /* JIT_OP_* (0..5) ≠ AST_CMP_* — يجب التحويل قبل استدعاء arp_value_cmp */
    static const int ast_ops[6] = {
        AST_CMP_EQ,  AST_CMP_NEQ, AST_CMP_LT,
        AST_CMP_LTE, AST_CMP_GT,  AST_CMP_GTE
    };

    arp_value_t a, b;
    int cmp = 0;
    int ok  = 0;

    arp_value_init(&a, ARP_TYPE_UNKNOWN);
    arp_value_init(&b, ARP_TYPE_UNKNOWN);

    if (op < 0 || op > 5) goto done;

    switch (g_jit_typed_trace_left.type) {
        case ARP_TYPE_TEXT:
            if (arp_value_make_text(&a, g_jit_typed_trace_left.text ? g_jit_typed_trace_left.text : "", -1) < 0) goto done;
            break;
        case ARP_TYPE_INT:    arp_value_make_int(&a, (int64_t)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_FLOAT:  arp_value_make_float(&a, (float)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_DOUBLE: arp_value_make_double(&a, g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_CHAR:   arp_value_make_char(&a, (uint32_t)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_BOOL:   arp_value_make_bool(&a, (int)g_jit_typed_trace_left.numeric); break;
        case ARP_TYPE_PTR:    arp_value_make_ptr(&a, (uintptr_t)(uint64_t)g_jit_typed_trace_left.numeric); break;
        default: goto done;
    }

    switch (g_jit_typed_trace.type) {
        case ARP_TYPE_TEXT:
            if (arp_value_make_text(&b, g_jit_typed_trace.text ? g_jit_typed_trace.text : "", -1) < 0) goto done;
            break;
        case ARP_TYPE_INT:    arp_value_make_int(&b, (int64_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_FLOAT:  arp_value_make_float(&b, (float)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_DOUBLE: arp_value_make_double(&b, g_jit_typed_trace.numeric); break;
        case ARP_TYPE_CHAR:   arp_value_make_char(&b, (uint32_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_BOOL:   arp_value_make_bool(&b, (int)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_PTR:    arp_value_make_ptr(&b, (uintptr_t)(uint64_t)g_jit_typed_trace.numeric); break;
        default: goto done;
    }

    if (arp_value_cmp(ast_ops[op], &a, &b, &cmp) == 0)
        ok = 1;

done:
    arp_value_free(&a);
    arp_value_free(&b);

    /* النتيجة دائمًا قيمة منطقية صريحة؛ فشل المقارنة = خطأ (0)،
     * لا قيمة الطرف الأيمن كما كان يحدث سابقًا. */
    jit_typed_trace_clear();
    g_jit_typed_trace.type    = ARP_TYPE_BOOL;
    g_jit_typed_trace.numeric = ok ? (double)cmp : 0.0;

    jit_typed_trace_left_clear();
}

static int jit_slot_to_value(const jit_arg_slot_t *s, arp_value_t *out)
{
    switch ((arp_type_t)s->type)
    {
        case ARP_TYPE_TEXT:
            return arp_value_make_text(out, s->text ? s->text : "", -1);
        case ARP_TYPE_INT:
            arp_value_make_int(out, (int64_t)s->numeric);
            return 0;
        case ARP_TYPE_FLOAT:
            arp_value_make_float(out, (float)s->numeric);
            return 0;
        case ARP_TYPE_DOUBLE:
            arp_value_make_double(out, s->numeric);
            return 0;
        case ARP_TYPE_CHAR:
            arp_value_make_char(out, (uint32_t)s->numeric);
            return 0;
        case ARP_TYPE_BOOL:
            arp_value_make_bool(out, (int)s->numeric);
            return 0;
        case ARP_TYPE_PTR:
            arp_value_make_ptr(out, (uintptr_t)(uint64_t)s->numeric);
            return 0;
        default:
            arp_value_make_double(out, s->numeric);
            return 0;
    }
}

__attribute__((used, noinline))
void jit_rt_typed_call_function(const char *name, int argc,
                                const jit_arg_slot_t *slots)
{
    if (!name || argc < 0 || argc > JIT_MAX_ARGC || (argc > 0 && !slots))
    {
        jit_typed_trace_clear();
        return;
    }

    /* حوّل المعاملات أولًا، قبل مسح الأثر: قد يملك الأثر نص آخر معامل */
    arp_value_t *arp_args = NULL;
    if (argc > 0)
    {
        arp_args = (arp_value_t *)memory_api.alloc((size_t)argc * sizeof(arp_value_t));
        if (!arp_args)
        {
            jit_typed_trace_clear();
            return;
        }
        for (int i = 0; i < argc; i++)
            arp_value_init(&arp_args[i], ARP_TYPE_UNKNOWN);

        for (int i = 0; i < argc; i++)
        {
            if (jit_slot_to_value(&slots[i], &arp_args[i]) < 0)
            {
                for (int k = 0; k < argc; k++) arp_value_free(&arp_args[k]);
                memory_api.free(arp_args);
                jit_typed_trace_clear();
                return;
            }
        }
    }

    jit_typed_trace_clear();

    arp_value_t result;
    arp_value_init(&result, ARP_TYPE_UNKNOWN);
    int rc = arp_invoke_function_typed(name, arp_args, argc, &result);

    if (arp_args)
    {
        for (int i = 0; i < argc; i++) arp_value_free(&arp_args[i]);
        memory_api.free(arp_args);
    }

    if (rc == 0 && result.type != ARP_TYPE_UNKNOWN)
    {
        jit_typed_trace_clear();
        g_jit_typed_trace.type = result.type;
        switch (result.type) {
            case ARP_TYPE_TEXT:
                g_jit_typed_trace.text = result.as.text;
                result.as.text = NULL;
                break;
            case ARP_TYPE_INT:    g_jit_typed_trace.numeric = (double)result.as.i64; break;
            case ARP_TYPE_FLOAT:  g_jit_typed_trace.numeric = (double)result.as.f32; break;
            case ARP_TYPE_DOUBLE: g_jit_typed_trace.numeric = result.as.f64; break;
            case ARP_TYPE_CHAR:   g_jit_typed_trace.numeric = (double)result.as.cp; break;
            case ARP_TYPE_BOOL:   g_jit_typed_trace.numeric = (double)result.as.b; break;
            case ARP_TYPE_PTR:    g_jit_typed_trace.numeric = (double)(uint64_t)result.as.ptr; break;
            default: break;
        }
    }
    arp_value_free(&result);
}

/* =========================================================
 * الأثر القديم (يبقى للطباعة والتوافق البسيط)
 * ========================================================= */

/* الطباعة: تستخدم g_jit_typed_trace مباشرة عبر المسار النوعي */

__attribute__((used, noinline))
double jit_rt_print_begin(void)
{
    g_print_pos = 0;
    g_print_buffer[0] = '\0';
    return 1.0;
}

__attribute__((used, noinline))
double jit_rt_set_print_color(double color_d)
{
    g_print_color = (uint32_t)(int64_t)color_d;
    return 1.0;
}

__attribute__((used, noinline))
double jit_rt_print_space(void)
{
    if (g_print_pos < JIT_PRINT_BUF_SIZE - 1)
    {
        g_print_buffer[g_print_pos++] = ' ';
        g_print_buffer[g_print_pos] = '\0';
    }
    return 1.0;
}

/* القيمة الحالية من الأثر النوعي */
static const char *jit_current_value_as_text(char *tmp, int tmp_size)
{
    if (g_jit_typed_trace.type == ARP_TYPE_TEXT)
        return g_jit_typed_trace.text ? g_jit_typed_trace.text : "";

    arp_value_t v;
    arp_value_init(&v, ARP_TYPE_UNKNOWN);

    switch (g_jit_typed_trace.type) {
        case ARP_TYPE_INT:    arp_value_make_int(&v, (int64_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_FLOAT:  arp_value_make_float(&v, (float)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_DOUBLE: arp_value_make_double(&v, g_jit_typed_trace.numeric); break;
        case ARP_TYPE_CHAR:   arp_value_make_char(&v, (uint32_t)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_BOOL:   arp_value_make_bool(&v, (int)g_jit_typed_trace.numeric); break;
        case ARP_TYPE_PTR:    arp_value_make_ptr(&v, (uintptr_t)(uint64_t)g_jit_typed_trace.numeric); break;
        default: v.type = ARP_TYPE_DOUBLE; v.as.f64 = g_jit_typed_trace.numeric; break;
    }

    char *s = NULL;
    const char *out = "";
    if (arp_value_to_text(&v, &s) == 0 && s) {
        int n = (int)strlen(s);
        if (n >= tmp_size) n = tmp_size - 1;
        memcpy(tmp, s, n);
        tmp[n] = '\0';
        out = tmp;
        memory_api.free(s);
    }
    arp_value_free(&v);
    return out;
}

__attribute__((used, noinline))
double jit_rt_print_value(void)
{
    char        tmp[128];
    const char *str = jit_current_value_as_text(tmp, sizeof(tmp));

    if (g_jit_typed_trace.type == ARP_TYPE_TEXT)
    {
        size_t len = strlen(str);
        if (g_print_pos + (int)len < JIT_PRINT_BUF_SIZE - 1)
        {
            memcpy(g_print_buffer + g_print_pos, str, len);
            g_print_pos += (int)len;
            g_print_buffer[g_print_pos] = '\0';
        }
    }
    else
    {
        /* أرقام عربية */
        while (*str)
        {
            if (*str >= '0' && *str <= '9')
            {
                const char *ad = g_arabic_digits[*str - '0'];
                if (g_print_pos + 2 < JIT_PRINT_BUF_SIZE - 1)
                {
                    g_print_buffer[g_print_pos++] = ad[0];
                    g_print_buffer[g_print_pos++] = ad[1];
                    g_print_buffer[g_print_pos]   = '\0';
                }
            }
            else
            {
                if (g_print_pos + 1 < JIT_PRINT_BUF_SIZE - 1)
                {
                    g_print_buffer[g_print_pos++] = *str;
                    g_print_buffer[g_print_pos]   = '\0';
                }
            }
            str++;
        }
    }

    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    return 1.0;
}

__attribute__((used, noinline))
double jit_rt_print_newline(void)
{
    execute_print_line_color(g_print_buffer, g_print_color);
    g_print_color = DEFAULT_PRINT_COLOR;
    g_print_pos = 0;
    g_print_buffer[0] = '\0';
    return 1.0;
}

__attribute__((used, noinline))
double jit_rt_print_flush_inline(void)
{
    execute_print_inline_color(g_print_buffer, g_print_color);
    g_print_color = DEFAULT_PRINT_COLOR;
    g_print_pos = 0;
    g_print_buffer[0] = '\0';
    return 1.0;
}

/* =========================================================
 * دوال وقت التشغيل — الذاكرة والنصوص والعتاد
 * ========================================================= */

__attribute__((used, noinline))
double jit_rt_text_len(const char *text)
{
    double n = text ? (double)(int)strlen(text) : 0.0;
    jit_typed_trace_clear();          /* بعد القراءة لأن النص قد يكون ملك الأثر */
    jit_typed_trace_left_clear();
    return jit_trace_return(ARP_TYPE_INT, n);
}


__attribute__((used, noinline))
double jit_rt_text_char(double text_as_double, double index_as_double)
{
    uint64_t p;
    memcpy(&p, &text_as_double, sizeof(p));
    const char *text = (const char *)(uintptr_t)p;
    if (!text) return -1.0;
    int index = (int)index_as_double;
    if (index < 0) return -1.0;
    int len = (int)strlen(text);
    if (index >= len) return -1.0;
    return (double)(unsigned char)text[index];
}

__attribute__((used, noinline))
double jit_rt_file_ptr(const char *name)
{
    uint64_t ptr = (name && name[0]) ? execute_file_pointer(name) : 0;
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    return jit_trace_return(ARP_TYPE_PTR, (double)ptr);
}

__attribute__((used, noinline))
double jit_rt_mem_read(int bits, double addr_d)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    uint64_t addr = (uint64_t)addr_d;
    switch (bits) {
        case 8:  { uint8_t  v = 0; if (execute_memory_read8 (addr, &v) < 0) return jit_trace_return(ARP_TYPE_INT, 0.0); return jit_trace_return(ARP_TYPE_INT, (double)v); }
        case 16: { uint16_t v = 0; if (execute_memory_read16(addr, &v) < 0) return jit_trace_return(ARP_TYPE_INT, 0.0); return jit_trace_return(ARP_TYPE_INT, (double)v); }
        case 32: { uint32_t v = 0; if (execute_memory_read  (addr, &v) < 0) return jit_trace_return(ARP_TYPE_INT, 0.0); return jit_trace_return(ARP_TYPE_INT, (double)(int32_t)v); }
        case 64: { uint64_t v = 0; if (execute_memory_read64(addr, &v) < 0) return jit_trace_return(ARP_TYPE_INT, 0.0); return jit_trace_return(ARP_TYPE_INT, (double)v); }
    }
    return jit_trace_return(ARP_TYPE_INT, 0.0);
}

__attribute__((used, noinline))
double jit_rt_mem_write(int bits, double addr_d, double val_d)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    uint64_t addr = (uint64_t)addr_d;
    int r = -1;
    switch (bits) {
        case 8:  r = execute_memory_write8 (addr, (uint8_t )val_d); break;
        case 16: r = execute_memory_write16(addr, (uint16_t)val_d); break;
        case 32: { int64_t v64 = (int64_t)val_d; r = execute_memory_write(addr, (uint32_t)v64); break; }
        case 64: r = execute_memory_write64(addr, (uint64_t)val_d); break;
    }
    return (r < 0) ? 0.0 : 1.0;
}

__attribute__((used, noinline))
double jit_rt_mem_copy(double dst_d, double src_d, double sz_d)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    uint64_t dst = (uint64_t)dst_d;
    uint64_t src = (uint64_t)src_d;
    return (execute_memory_copy(dst, src, (uint64_t)sz_d) < 0) ? 0.0 : 1.0;
}

__attribute__((used, noinline))
double jit_rt_mem_set(double addr_d, double val_d, double sz_d)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    uint64_t addr = (uint64_t)addr_d;
    return (execute_memory_set(addr, (uint8_t)val_d, (uint64_t)sz_d) < 0) ? 0.0 : 1.0;
}

__attribute__((used, noinline))
double jit_rt_mem_set32(double addr_d, double val_d, double count_d)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    uint64_t addr = (uint64_t)addr_d;
    return (execute_memory_set32(addr, (uint32_t)val_d, (uint64_t)count_d) < 0) ? 0.0 : 1.0;
}

__attribute__((used, noinline))
double jit_rt_mem_compare(double a1_d, double a2_d, double sz_d)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    uint64_t a1 = (uint64_t)a1_d;
    uint64_t a2 = (uint64_t)a2_d;
    int cmp = 0;
    if (execute_memory_compare(a1, a2, (uint64_t)sz_d, &cmp) < 0) return jit_trace_return(ARP_TYPE_INT, 0.0);
    return jit_trace_return(ARP_TYPE_INT, (double)cmp);
}

__attribute__((used, noinline))
double jit_rt_mem_alloc(double sz_d)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    uint64_t addr = 0;
    if (execute_memory_alloc((uint64_t)sz_d, &addr) < 0) return jit_trace_return(ARP_TYPE_PTR, 0.0);
    return jit_trace_return(ARP_TYPE_PTR, (double)addr);
}

__attribute__((used, noinline))
double jit_rt_mem_free(double addr_d)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    uint64_t addr = (uint64_t)addr_d;
    return (execute_memory_free(addr) < 0) ? 0.0 : 1.0;
}

__attribute__((used, noinline))
double jit_rt_mem_alloc_page(void)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    uint64_t addr = 0;
    if (execute_memory_alloc_page(&addr) < 0) return jit_trace_return(ARP_TYPE_PTR, 0.0);
    return jit_trace_return(ARP_TYPE_PTR, (double)addr);
}

__attribute__((used, noinline))
double jit_rt_mem_map(double virt_d, double phys_d, double flags_d)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    uint64_t virt = (uint64_t)virt_d;
    uint64_t phys = (uint64_t)phys_d;
    return (execute_memory_map(virt, phys, (uint64_t)flags_d) < 0) ? 0.0 : 1.0;
}

__attribute__((used, noinline))
double jit_rt_hw_out(double port_d, double val_d)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    execute_hardware((uint16_t)port_d, (uint8_t)val_d);
    return 1.0;
}

__attribute__((used, noinline))
double jit_rt_hw_in(double port_d)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    return jit_trace_return(ARP_TYPE_INT, (double)execute_read_hardware((uint16_t)port_d));
}

__attribute__((used, noinline))
double jit_rt_wait(double count_d)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    return (execute_wait((uint64_t)count_d) < 0) ? 0.0 : 1.0;
}

__attribute__((used, noinline))
double jit_rt_input(const char *name)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    if (!name) return 0.0;
    return (execute_input(name) < 0) ? 0.0 : 1.0;
}

__attribute__((used, noinline))
double jit_rt_keyboard_cond(const char *line)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    if (!line) return 0.0;
    return (condition_evaluate(line) > 0) ? 1.0 : 0.0;
}

__attribute__((used, noinline))
double jit_rt_app(const char *name)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    if (!name) return 0.0;
    execute_app(name);
    return 1.0;
}

__attribute__((used, noinline))
double jit_rt_lib_import(const char *name)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    if (!name) return 0.0;
    return (library_import(name) < 0) ? 0.0 : 1.0;
}

__attribute__((used, noinline))
void jit_rt_create_struct(const char *type_name, const char *instance_name)
{
    if (!type_name || !instance_name) return;
    int fc = struct_field_count(type_name);
    if (fc <= 0) return;

    for (int i = 0; i < fc; i++)
    {
        const char       *fname = struct_field_name_at(type_name, i);
        const arp_value_t *fdef  = struct_field_default_at(type_name, i);
        if (!fname || !fdef) continue;

        int l1 = (int)strlen(instance_name);
        int l2 = (int)strlen(fname);
        char *full = (char *)memory_api.alloc((size_t)l1 + 1 + l2 + 1);
        if (!full) return;
        memcpy(full, instance_name, l1);
        full[l1] = '.';
        memcpy(full + l1 + 1, fname, l2);
        full[l1 + 1 + l2] = '\0';

        variable_set_value(full, fdef);
        memory_api.free(full);
    }
}

__attribute__((used, noinline))
double jit_rt_present(void)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    return (execute_present_screen() < 0) ? 0.0 : 1.0;
}


__attribute__((used, noinline))
double jit_rt_mem_write_addr(int bits, uint64_t addr, uint64_t val)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    int r = -1;
    switch (bits) {
        case 8:  r = execute_memory_write8 (addr, (uint8_t)val); break;
        case 16: r = execute_memory_write16(addr, (uint16_t)val); break;
        case 32: r = execute_memory_write(addr, (uint32_t)val); break;
        case 64: r = execute_memory_write64(addr, val); break;
    }
    return (r < 0) ? 0.0 : 1.0;
}

__attribute__((used, noinline))
double jit_rt_mem_set32_addr(uint64_t addr, uint32_t val, uint64_t count)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    return (execute_memory_set32(addr, val, count) < 0) ? 0.0 : 1.0;
}


__attribute__((used, noinline))
double jit_rt_byte_at_abi(uint64_t addr)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    return jit_trace_return(ARP_TYPE_INT, (double)execute_byte_at(addr));
}


/* ★ قراءة فهرس مصفوفة (للقراءة فقط حاليًا).
 *   تستقبل الآن خانات نوعية (24 بايت) لا int64 خام. */
__attribute__((used, noinline))
void jit_rt_array_index(const char *name, int argc, const jit_arg_slot_t *slots)
{
    if (!name || argc < 1 || argc > 8 || !slots)
    {
        jit_typed_trace_clear();
        return;
    }

    int idx[8];
    for (int i = 0; i < argc; i++) idx[i] = (int)slots[i].numeric;

    jit_typed_trace_clear();

    arp_value_t holder;
    arp_value_init(&holder, ARP_TYPE_UNKNOWN);
    if (variable_get_value(name, &holder) < 0) return;
    if (holder.type != ARP_TYPE_ARRAY)
    {
        arp_value_free(&holder);
        return;
    }

    arp_value_t v;
    arp_value_init(&v, ARP_TYPE_UNKNOWN);
    int rc = arp_array_get(holder.as.array, idx, argc, &v);
    arp_value_free(&holder);

    if (rc < 0) { arp_value_free(&v); return; }

    jit_typed_trace_clear();
    switch (v.type) {
        case ARP_TYPE_TEXT:
            g_jit_typed_trace.type = ARP_TYPE_TEXT;
            g_jit_typed_trace.text = v.as.text;
            v.as.text = NULL;
            break;
        case ARP_TYPE_INT:    g_jit_typed_trace.type = ARP_TYPE_INT;    g_jit_typed_trace.numeric = (double)v.as.i64; break;
        case ARP_TYPE_FLOAT:  g_jit_typed_trace.type = ARP_TYPE_FLOAT;  g_jit_typed_trace.numeric = (double)v.as.f32; break;
        case ARP_TYPE_DOUBLE: g_jit_typed_trace.type = ARP_TYPE_DOUBLE; g_jit_typed_trace.numeric = v.as.f64; break;
        case ARP_TYPE_CHAR:   g_jit_typed_trace.type = ARP_TYPE_CHAR;   g_jit_typed_trace.numeric = (double)v.as.cp; break;
        case ARP_TYPE_BOOL:   g_jit_typed_trace.type = ARP_TYPE_BOOL;   g_jit_typed_trace.numeric = (double)v.as.b; break;
        case ARP_TYPE_PTR:    g_jit_typed_trace.type = ARP_TYPE_PTR;    g_jit_typed_trace.numeric = (double)(uint64_t)v.as.ptr; break;
        default: break;
    }
    arp_value_free(&v);
}


/* =========================================================
 * ★★★ دوال ذاكرة ABI صحيح: العنوان uint64_t كامل الدقة ★★★
 * ========================================================= */

__attribute__((used, noinline))
double jit_rt_mem_write32_abi(uint64_t addr, uint32_t val)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    return (execute_memory_write(addr, val) < 0) ? 0.0 : 1.0;
}

__attribute__((used, noinline))
double jit_rt_mem_write8_abi(uint64_t addr, uint32_t val)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    return (execute_memory_write8(addr, (uint8_t)val) < 0) ? 0.0 : 1.0;
}

__attribute__((used, noinline))
double jit_rt_mem_write16_abi(uint64_t addr, uint32_t val)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    return (execute_memory_write16(addr, (uint16_t)val) < 0) ? 0.0 : 1.0;
}

__attribute__((used, noinline))
double jit_rt_mem_write64_abi(uint64_t addr, uint64_t val)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();
    return (execute_memory_write64(addr, val) < 0) ? 0.0 : 1.0;
}

__attribute__((used, noinline))
double jit_rt_mem_set32_abi(uint64_t addr, uint32_t val, uint64_t count)
{
    jit_typed_trace_clear();
    jit_typed_trace_left_clear();

    /* حماية: ارفض العناوين غير القانونية أو الأعداد الضخمة بدل إسقاط النواة بـ #GP */
    if (addr < 0x100000ULL || addr >= 0x0000800000000000ULL ||
        count > 0x1000000ULL || (addr + count * 4ULL) < addr) {
        return 0.0;
    }

    return (execute_memory_set32(addr, val, count) < 0) ? 0.0 : 1.0;
}

/* =========================================================
 * جدول الرموز
 * ========================================================= */

typedef struct {
    const char *name;
    void       *addr;
} jit_symbol_t;

#define JIT_SYM(fn) { #fn, (void *)(fn) }

static const jit_symbol_t g_jit_symbols[] = {
    /* الأثر النوعي */
    JIT_SYM(jit_rt_typed_get_variable),
    JIT_SYM(jit_rt_typed_set_variable),
    JIT_SYM(jit_rt_typed_declare_variable),
    JIT_SYM(jit_rt_declare_array),
    JIT_SYM(jit_rt_array_index),
    JIT_SYM(jit_rt_typed_capture_left),
    JIT_SYM(jit_rt_typed_add),
    JIT_SYM(jit_rt_typed_sub),
    JIT_SYM(jit_rt_typed_mul),
    JIT_SYM(jit_rt_typed_div),
    JIT_SYM(jit_rt_typed_neg),
    JIT_SYM(jit_rt_typed_compare),
    JIT_SYM(jit_rt_typed_and),
    JIT_SYM(jit_rt_typed_or),   
    JIT_SYM(jit_rt_typed_call_function),
    JIT_SYM(jit_rt_typed_trace_from_regs),
    JIT_SYM(jit_rt_create_struct),

    JIT_SYM(jit_rt_typed_div),
    JIT_SYM(jit_rt_typed_mod),
    JIT_SYM(jit_rt_typed_shl),
    JIT_SYM(jit_rt_typed_shr),
    JIT_SYM(jit_rt_typed_bit_and),
    JIT_SYM(jit_rt_typed_bit_or),
    JIT_SYM(jit_rt_typed_bit_xor),
    JIT_SYM(jit_rt_typed_neg),

    /* نصوص وملفات */
    JIT_SYM(jit_rt_text_len),
    JIT_SYM(jit_rt_text_char),
    JIT_SYM(jit_rt_file_ptr),

    /* طباعة */
    JIT_SYM(jit_rt_set_print_color),
    JIT_SYM(jit_rt_print_begin),
    JIT_SYM(jit_rt_print_value),
    JIT_SYM(jit_rt_print_space),
    JIT_SYM(jit_rt_print_newline),
    JIT_SYM(jit_rt_print_flush_inline),

    /* ذاكرة */
    JIT_SYM(jit_rt_mem_read),
    JIT_SYM(jit_rt_mem_write),
    JIT_SYM(jit_rt_mem_copy),
    JIT_SYM(jit_rt_mem_set),
    JIT_SYM(jit_rt_mem_set32),
    JIT_SYM(jit_rt_mem_compare),
    JIT_SYM(jit_rt_mem_alloc),
    JIT_SYM(jit_rt_mem_free),
    JIT_SYM(jit_rt_mem_alloc_page),
    JIT_SYM(jit_rt_mem_map),
    JIT_SYM(jit_rt_byte_at_abi),

    /* عتاد وإدخال */
    JIT_SYM(jit_rt_hw_out),
    JIT_SYM(jit_rt_hw_in),
    JIT_SYM(jit_rt_wait),
    JIT_SYM(jit_rt_input),
    JIT_SYM(jit_rt_keyboard_cond),
    JIT_SYM(jit_rt_app),
    JIT_SYM(jit_rt_lib_import),

    JIT_SYM(jit_rt_present),

    JIT_SYM(jit_rt_mem_write_addr),
    JIT_SYM(jit_rt_mem_set32_addr),

    JIT_SYM(jit_rt_mem_write32_abi),
    JIT_SYM(jit_rt_mem_write8_abi),
    JIT_SYM(jit_rt_mem_write16_abi),
    JIT_SYM(jit_rt_mem_write64_abi),
    JIT_SYM(jit_rt_mem_set32_abi),

    JIT_SYM(jit_rt_typed_return_from_regs),

    /* بيانات (وليست دوال): يحتاجها jit_emit_load_typed_result في وضع AOT.
     * يجب أن تبقى في آخر الجدول كي لا تتغير أرقام الرموز الأخرى. */
    { "g_jit_typed_trace", (void *)&g_jit_typed_trace },
};

#undef JIT_SYM

#define JIT_SYMBOL_COUNT \
    ((uint32_t)(sizeof(g_jit_symbols) / sizeof(g_jit_symbols[0])))

static int jit_helper_symbol_id(const void *fn)
{
    for (uint32_t i = 0; i < JIT_SYMBOL_COUNT; i++)
        if (g_jit_symbols[i].addr == fn) return (int)i;
    return -1;
}

void *machine_jit_resolve_symbol(uint32_t symbol)
{
    if (symbol >= JIT_SYMBOL_COUNT) return NULL;
    return g_jit_symbols[symbol].addr;
}

uint32_t machine_jit_symbol_count(void)
{
    return JIT_SYMBOL_COUNT;
}

/* =========================================================
 * فحص قابلية JIT
 * ========================================================= */

static int jit_check_call_builtin(const ast_node_t *node, const char *fname)
{
    if (strcmp(fname, "اعرض_الشاشة") == 0)
        return node->function_call.arg_count == 0;

    if (strcmp(fname, "طول_النص") == 0 || strcmp(fname, "ملف_مؤشر") == 0 ||
        strcmp(fname, "ذاكرة_اقرا") == 0 || strcmp(fname, "ذاكرة_اقرا8") == 0 ||
        strcmp(fname, "ذاكرة_اقرا16") == 0 || strcmp(fname, "ذاكرة_اقرا32") == 0 ||
        strcmp(fname, "ذاكرة_اقرا64") == 0 || strcmp(fname, "ذاكرة_خصص") == 0 ||
        strcmp(fname, "ذاكرة_حرر") == 0 || strcmp(fname, "اقرا_عتاد") == 0 ||
        strcmp(fname, "انتظر") == 0)
    {
        if (node->function_call.arg_count != 1) return 0;
        return machine_ast_is_jit_supported(node->function_call.args[0]);
    }

    if (strcmp(fname, "اقرا_بايت") == 0)
    {
        if (node->function_call.arg_count != 1) return 0;
        return machine_ast_is_jit_supported(node->function_call.args[0]);
    }

    if (strcmp(fname, "ذاكرة_خصص_صفحة") == 0)
        return node->function_call.arg_count == 0;

    if (strcmp(fname, "عتاد") == 0 || strcmp(fname, "حرف_النص") == 0)
    {
        if (node->function_call.arg_count != 2) return 0;
        return machine_ast_is_jit_supported(node->function_call.args[0]) &&
               machine_ast_is_jit_supported(node->function_call.args[1]);
    }

    if (strcmp(fname, "ذاكرة_اكتب") == 0 || strcmp(fname, "ذاكرة_اكتب8") == 0 ||
        strcmp(fname, "ذاكرة_اكتب16") == 0 || strcmp(fname, "ذاكرة_اكتب32") == 0 ||
        strcmp(fname, "ذاكرة_اكتب64") == 0)
    {
        if (node->function_call.arg_count != 2) return 0;
        return machine_ast_is_jit_supported(node->function_call.args[0]) &&
               machine_ast_is_jit_supported(node->function_call.args[1]);
    }

    if (strcmp(fname, "ذاكرة_انسخ") == 0 || strcmp(fname, "ذاكرة_املأ") == 0 ||
        strcmp(fname, "ذاكرة_املأ32") == 0 || strcmp(fname, "ذاكرة_قارن") == 0 ||
        strcmp(fname, "ذاكرة_اربط") == 0)
    {
        if (node->function_call.arg_count != 3) return 0;
        for (int i = 0; i < 3; i++)
            if (!machine_ast_is_jit_supported(node->function_call.args[i])) return 0;
        return 1;
    }

    if (strcmp(fname, "ادخل") == 0)
    {
        if (node->function_call.arg_count != 1) return 0;
        return node->function_call.args[0]->type == AST_VARIABLE;
    }

    if (strcmp(fname, "اطبع") == 0 || strcmp(fname, "اطبع_سطر") == 0)
    {
        if (node->function_call.arg_count < 1) return 0;
        for (int i = 0; i < node->function_call.arg_count; i++)
            if (!machine_ast_is_jit_supported(node->function_call.args[i])) return 0;
        return 1;
    }

    if (strcmp(fname, "شغل") == 0)
    {
        if (node->function_call.arg_count != 1) return 0;
        ast_node_type_t t = node->function_call.args[0]->type;
        return (t == AST_VARIABLE || t == AST_STRING);
    }

    return -1;
}

int machine_ast_is_jit_supported(const ast_node_t *node)
{
    if (!node) return 0;
    switch (node->type)
    {
        case AST_NUMBER:
        case AST_VARIABLE:
        case AST_STRING:
            return 1;
        case AST_NEG:
            return machine_ast_is_jit_supported(node->unary.operand);
        case AST_ADD:
            return machine_ast_is_jit_supported(node->binary.left) &&
                   machine_ast_is_jit_supported(node->binary.right);

        case AST_SUB: case AST_MUL: case AST_DIV:
        case AST_MOD: case AST_SHL: case AST_SHR:
        case AST_BIT_AND: case AST_BIT_OR: case AST_BIT_XOR:
            if (node->binary.left->type  == AST_STRING ||
                node->binary.right->type == AST_STRING) return 0;
            return machine_ast_is_jit_supported(node->binary.left) &&
                   machine_ast_is_jit_supported(node->binary.right);

        case AST_ASSIGN:
            return machine_ast_is_jit_supported(node->assignment.value);
        case AST_CMP_EQ: case AST_CMP_NEQ:
        case AST_CMP_LT: case AST_CMP_LTE:
        case AST_CMP_GT: case AST_CMP_GTE:
        case AST_AND: case AST_OR:                     
            return machine_ast_is_jit_supported(node->binary.left) &&
                   machine_ast_is_jit_supported(node->binary.right);

        case AST_INDEX:
        {
            for (int i = 0; i < node->array_index.index_count; i++)
                if (!machine_ast_is_jit_supported(node->array_index.indices[i]))
                    return 0;
            return 1;
        }
        case AST_ARRAY_LIT:
        {
            for (int i = 0; i < node->array_lit.element_count; i++)
                if (!machine_ast_is_jit_supported(node->array_lit.elements[i]))
                    return 0;
            return 1;
        }

        case AST_FUNCTION_CALL:
        {
            const char *fname = node->function_call.name;
            int builtin = jit_check_call_builtin(node, fname);
            if (builtin >= 0) return builtin;
            if (!function_find(fname)) return 0;
            if (node->function_call.arg_count > JIT_MAX_ARGC) return 0;
            for (int i = 0; i < node->function_call.arg_count; i++)
                if (!machine_ast_is_jit_supported(node->function_call.args[i])) return 0;
            return 1;
        }
        default: return 0;
    }
}

/* =========================================================
 * ★★★ المسار النوعي — التعبيرات ★★★
 * ========================================================= */

static int jit_compile_number_typed(const ast_node_t *node, jit_buf_t *b, int *depth)
{
    (void)depth;
    double d = node->number;
    int is_int = (d == (double)(int64_t)d &&
                  d >= -9223372036854775808.0 &&
                  d <  9223372036854775808.0);
    if (jit_emit_load_double(b, d) < 0) return -1;
    if (jit_emit_mov_edx_imm(b, is_int ? ARP_TYPE_INT : ARP_TYPE_DOUBLE) < 0) return -1;
    if (jit_emit_xor_rcx_rcx(b) < 0) return -1;
    return 0;
}

static int jit_compile_string_typed(const ast_node_t *node, jit_buf_t *b, int *depth)
{
    (void)depth;
    const char *s = node->text ? node->text : "";
    if (jit_emit_name_ref(b, s) < 0) return -1;
    if (jit_emit_mov_rcx_rdi(b) < 0) return -1;
    if (jit_emit_xorpd_xmm0_xmm0(b) < 0) return -1;
    if (jit_emit_mov_edx_imm(b, ARP_TYPE_TEXT) < 0) return -1;
    return 0;
}

static int jit_compile_variable_typed(const ast_node_t *node, jit_buf_t *b, int *depth)
{
    (void)depth;
    if (jit_emit_name_ref(b, node->name) < 0) return -1;
    if (jit_emit_call_helper(b, (const void *)jit_rt_typed_get_variable) < 0) return -1;
    return jit_emit_load_typed_result(b);
}

static int jit_compile_assign_typed(const ast_node_t *node, jit_buf_t *b, int *depth)
{
    if (jit_compile_expr_typed(node->assignment.value, b, depth) < 0) return -1;

    if (*depth >= JIT_MAX_DEPTH) return -1;
    if (jit_emit_store_typed_slot(b, *depth) < 0) return -1;
    (*depth)++;

    if (jit_emit_name_ref(b, node->assignment.name) < 0) return -1;
    if (jit_emit_load_typed_slot(b, *depth - 1) < 0) return -1;
    if (jit_emit_call_helper(b, (const void *)jit_rt_typed_trace_from_regs) < 0) return -1;
    if (jit_emit_call_helper(b, (const void *)jit_rt_typed_set_variable) < 0) return -1;

    (*depth)--;
    return jit_emit_load_typed_slot(b, *depth);
}

/* يقيّم الطرفين ويترك: اليسار في g_jit_typed_trace_left، واليمين في g_jit_typed_trace.
 * الطرفان يُحفظان في slots على المكدس لأن الطرف الأيمن قد يكون عملية ثنائية
 * أخرى (فيمسح g_jit_typed_trace_left المشترك). */
static int jit_compile_operands_typed(const ast_node_t *node, jit_buf_t *b, int *depth)
{
    int base = *depth;
    if (base + 2 > JIT_MAX_DEPTH) return -1;

    if (jit_compile_expr_typed(node->binary.left, b, depth) < 0) { *depth = base; return -1; }
    if (jit_emit_store_typed_slot(b, base) < 0)                  { *depth = base; return -1; }
    *depth = base + 1;

    if (jit_compile_expr_typed(node->binary.right, b, depth) < 0) { *depth = base; return -1; }
    if (jit_emit_store_typed_slot(b, base + 1) < 0)               { *depth = base; return -1; }
    *depth = base + 2;

    /* اليسار: slot → trace → left */
    if (jit_emit_load_typed_slot(b, base) < 0) return -1;
    if (jit_emit_call_helper(b, (const void *)jit_rt_typed_trace_from_regs) < 0) return -1;
    if (jit_emit_call_helper(b, (const void *)jit_rt_typed_capture_left) < 0) return -1;

    /* اليمين: slot → trace */
    if (jit_emit_load_typed_slot(b, base + 1) < 0) return -1;
    if (jit_emit_call_helper(b, (const void *)jit_rt_typed_trace_from_regs) < 0) return -1;

    *depth = base;
    return 0;
}

static int jit_compile_compare_typed(const ast_node_t *node, jit_buf_t *b, int *depth)
{
    if (jit_compile_operands_typed(node, b, depth) < 0) return -1;

    int op = JIT_OP_EQ;
    if      (node->type == AST_CMP_EQ)  op = JIT_OP_EQ;
    else if (node->type == AST_CMP_NEQ) op = JIT_OP_NEQ;
    else if (node->type == AST_CMP_LT)  op = JIT_OP_LT;
    else if (node->type == AST_CMP_LTE) op = JIT_OP_LTE;
    else if (node->type == AST_CMP_GT)  op = JIT_OP_GT;
    else if (node->type == AST_CMP_GTE) op = JIT_OP_GTE;
    if (jit_emit_mov_edi_imm(b, (uint32_t)op) < 0) return -1;

    if (jit_emit_call_helper(b, (const void *)jit_rt_typed_compare) < 0) return -1;
    return jit_emit_load_typed_result(b);
}

static int jit_compile_add_typed(const ast_node_t *node, jit_buf_t *b, int *depth)
{
    if (jit_compile_operands_typed(node, b, depth) < 0) return -1;
    if (jit_emit_call_helper(b, (const void *)jit_rt_typed_add) < 0) return -1;
    return jit_emit_load_typed_result(b);
}

static int jit_compile_binop_typed(const ast_node_t *node, jit_buf_t *b, int *depth,
                                   const void *helper)
{
    if (jit_compile_operands_typed(node, b, depth) < 0) return -1;
    if (jit_emit_call_helper(b, helper) < 0) return -1;
    return jit_emit_load_typed_result(b);
}

static int jit_compile_neg_typed(const ast_node_t *node, jit_buf_t *b, int *depth)
{
    if (jit_compile_expr_typed(node->unary.operand, b, depth) < 0) return -1;
    if (jit_emit_call_helper(b, (const void *)jit_rt_typed_trace_from_regs) < 0) return -1;
    if (jit_emit_call_helper(b, (const void *)jit_rt_typed_neg) < 0) return -1;
    return jit_emit_load_typed_result(b);
}

/* دوال بوسيط رقمي واحد */
static int jit_compile_simple_unary_typed(const ast_node_t *node, jit_buf_t *b,
                                          int *depth, const void *helper)
{
    if (node->function_call.arg_count != 1) return -1;
    if (jit_compile_expr_typed(node->function_call.args[0], b, depth) < 0) return -1;
    if (jit_emit_call_helper(b, helper) < 0) return -1;
    return jit_emit_load_typed_result(b);
}

static int jit_compile_name_unary_typed(const ast_node_t *node, jit_buf_t *b,
                                        const void *helper)
{
    if (node->function_call.arg_count != 1) return -1;
    const ast_node_t *arg = node->function_call.args[0];
    const char *name = NULL;
    if (arg->type == AST_VARIABLE)      name = arg->name;
    else if (arg->type == AST_STRING)   name = arg->text;
    else return -1;
    if (!name) return -1;
    if (jit_emit_name_ref(b, name) < 0) return -1;
    if (jit_emit_call_helper(b, helper) < 0) return -1;
    return jit_emit_load_typed_result(b);
}

static int jit_emit_mov_rsi_rax(jit_buf_t *b)
{
    static const uint8_t c[] = { 0x48, 0x89, 0xC6 };
    return jit_emit_bytes(b, c, sizeof(c));
}

static int jit_emit_mov_rdi_rax(jit_buf_t *b)
{
    static const uint8_t c[] = { 0x48, 0x89, 0xC7 };
    return jit_emit_bytes(b, c, sizeof(c));
}

static int jit_emit_mov_rdx_rax(jit_buf_t *b)
{
    static const uint8_t c[] = { 0x48, 0x89, 0xC2 };
    return jit_emit_bytes(b, c, sizeof(c));
}



/* ذاكرة خام */
static int jit_compile_memory_fn_typed(const ast_node_t *node, jit_buf_t *b, int *depth)
{
    const char *fname = node->function_call.name;
    int nargs = -1;
    const void *helper = NULL;
    int addr_in_rdi = 0;

    /* --- مسار ABI صحيح: العنوان uint64_t في rdi --- */
    if (strcmp(fname, "ذاكرة_اكتب8")  == 0) { nargs = 2; helper = (const void *)jit_rt_mem_write8_abi;  addr_in_rdi = 1; }
    else if (strcmp(fname, "ذاكرة_اكتب16") == 0) { nargs = 2; helper = (const void *)jit_rt_mem_write16_abi; addr_in_rdi = 1; }
    else if (strcmp(fname, "ذاكرة_اكتب32") == 0) { nargs = 2; helper = (const void *)jit_rt_mem_write32_abi; addr_in_rdi = 1; }
    else if (strcmp(fname, "ذاكرة_اكتب64") == 0) { nargs = 2; helper = (const void *)jit_rt_mem_write64_abi; addr_in_rdi = 1; }
    else if (strcmp(fname, "ذاكرة_اكتب")   == 0) { nargs = 2; helper = (const void *)jit_rt_mem_write32_abi; addr_in_rdi = 1; }
    else if (strcmp(fname, "ذاكرة_املأ32") == 0) { nargs = 3; helper = (const void *)jit_rt_mem_set32_abi;    addr_in_rdi = 1; }
    else if (strcmp(fname, "اقرا_بايت") == 0)
    {
        nargs = 1;
        helper = (const void *)jit_rt_byte_at_abi;
        addr_in_rdi = 1;
    }

    /* --- المسار القديم: double في xmm --- */
    else if (strcmp(fname, "ذاكرة_اقرا8")  == 0) { nargs = 1; helper = (const void *)jit_rt_mem_read; }
    else if (strcmp(fname, "ذاكرة_اقرا16") == 0) { nargs = 1; helper = (const void *)jit_rt_mem_read; }
    else if (strcmp(fname, "ذاكرة_اقرا32") == 0) { nargs = 1; helper = (const void *)jit_rt_mem_read; }
    else if (strcmp(fname, "ذاكرة_اقرا64") == 0) { nargs = 1; helper = (const void *)jit_rt_mem_read; }
    else if (strcmp(fname, "ذاكرة_اقرا")   == 0) { nargs = 1; helper = (const void *)jit_rt_mem_read; }
    else if (strcmp(fname, "ذاكرة_انسخ")   == 0) { nargs = 3; helper = (const void *)jit_rt_mem_copy; }
    else if (strcmp(fname, "ذاكرة_املأ")   == 0) { nargs = 3; helper = (const void *)jit_rt_mem_set; }
    else if (strcmp(fname, "ذاكرة_قارن")   == 0) { nargs = 3; helper = (const void *)jit_rt_mem_compare; }
    else if (strcmp(fname, "ذاكرة_اربط")   == 0) { nargs = 3; helper = (const void *)jit_rt_mem_map; }
    else if (strcmp(fname, "ذاكرة_خصص")    == 0) { nargs = 1; helper = (const void *)jit_rt_mem_alloc; }
    else if (strcmp(fname, "ذاكرة_حرر")    == 0) { nargs = 1; helper = (const void *)jit_rt_mem_free; }
    else if (strcmp(fname, "ذاكرة_خصص_صفحة") == 0) { nargs = 0; helper = (const void *)jit_rt_mem_alloc_page; }
    else return -1;

    if (node->function_call.arg_count != nargs) return -1;

    if (addr_in_rdi)
    {
        /*
         * نُقيّم المعاملات واحدًا واحدًا، ونحوّل كل واحدة إلى int64
         * في السجل المطلوب مباشرةً:
         *   nargs==2: rdi = arg0 (عنوان), rsi = arg1 (قيمة)
         *   nargs==3: rdi = arg0 (عنوان), rsi = arg1 (قيمة), rdx = arg2 (عدد)
         *
         * نستخدم RAX كسجل مؤقت: نقيم التعبير → xmm0 → rax → rsi/rdx/rdi.
         * لكن يجب تقييم التعبير دون أن يمسح السجلات، لذا نستخدم slots.
         */

        int base = *depth;
        if (base + nargs > JIT_MAX_DEPTH) return -1;

        /* قيّم كل معامل وخزّنه في slot */
        for (int i = 0; i < nargs; i++)
        {
            if (jit_compile_expr_typed(node->function_call.args[i], b, depth) < 0)
                { *depth = base; return -1; }
            if (jit_emit_store_typed_slot(b, *depth) < 0)
                { *depth = base; return -1; }
            (*depth)++;
        }
        *depth = base;

        /* ★ jit_emit_load_typed_slot يكتب xmm0 و edx و rcx معًا،
         *   لذلك أي سجل مُعدّ سابقًا في edx/rcx يُفسَد. نحمّل rdi ثم rsi،
         *   ونحمّل rdx (المعامل الثالث) أخيرًا حتى لا يُكتب فوقه النوع. */
        if (jit_emit_load_typed_slot(b, base + 0) < 0) { *depth = base; return -1; }
        if (jit_emit_cvttsd2si_rax_xmm0(b) < 0)        { *depth = base; return -1; }
        if (jit_emit_mov_rdi_rax(b) < 0)               { *depth = base; return -1; }

        if (jit_emit_load_typed_slot(b, base + 1) < 0) { *depth = base; return -1; }
        if (jit_emit_cvttsd2si_rax_xmm0(b) < 0)        { *depth = base; return -1; }
        if (jit_emit_mov_rsi_rax(b) < 0)               { *depth = base; return -1; }

        if (nargs == 3)
        {
            /* آخر تحميل: edx يُكتب بالنوع ثم يُستبدل فورًا بقيمة rdx الصحيحة */
            if (jit_emit_load_typed_slot(b, base + 2) < 0) { *depth = base; return -1; }
            if (jit_emit_cvttsd2si_rax_xmm0(b) < 0)        { *depth = base; return -1; }
            if (jit_emit_mov_rdx_rax(b) < 0)               { *depth = base; return -1; }
        }

        if (jit_emit_call_helper(b, helper) < 0) return -1;
        return jit_emit_load_typed_result(b);
    }

    /* --- المسار القديم: double في xmm0/xmm1/xmm2 --- */
    int bits = 0;
    if      (strcmp(fname, "ذاكرة_اقرا8")  == 0 || strcmp(fname, "ذاكرة_اقرا16") == 0 ||
             strcmp(fname, "ذاكرة_اقرا32") == 0 || strcmp(fname, "ذاكرة_اقرا64") == 0 ||
             strcmp(fname, "ذاكرة_اقرا")   == 0) bits = 32; /* غير مستخدم فعلياً هنا */
    (void)bits;

    if (nargs > 0)
    {
        int base = *depth;
        if (base + nargs > JIT_MAX_DEPTH) return -1;
        for (int i = 0; i < nargs; i++)
        {
            if (jit_compile_expr_typed(node->function_call.args[i], b, depth) < 0)
                { *depth = base; return -1; }
            if (jit_emit_store_slot(b, *depth) < 0)
                { *depth = base; return -1; }
            (*depth)++;
        }
        if (nargs == 1)      { if (jit_emit_load_slot_xmm0(b, base) < 0)     { *depth = base; return -1; } }
        else if (nargs == 2) { if (jit_emit_load_slot_xmm1(b, base + 1) < 0) { *depth = base; return -1; }
                               if (jit_emit_load_slot_xmm0(b, base) < 0)     { *depth = base; return -1; } }
        else                 { if (jit_emit_load_slot_xmm2(b, base + 2) < 0) { *depth = base; return -1; }
                               if (jit_emit_load_slot_xmm1(b, base + 1) < 0) { *depth = base; return -1; }
                               if (jit_emit_load_slot_xmm0(b, base) < 0)     { *depth = base; return -1; } }
        *depth = base;
    }

    /* bits للقراءة: نمرر في edi (لأن دوال read الحالية تستقبل bits في edi) */
    if (strcmp(fname, "ذاكرة_اقرا8")  == 0) { if (jit_emit_mov_edi_imm(b, 8)  < 0) return -1; }
    else if (strcmp(fname, "ذاكرة_اقرا16") == 0) { if (jit_emit_mov_edi_imm(b, 16) < 0) return -1; }
    else if (strcmp(fname, "ذاكرة_اقرا32") == 0) { if (jit_emit_mov_edi_imm(b, 32) < 0) return -1; }
    else if (strcmp(fname, "ذاكرة_اقرا64") == 0) { if (jit_emit_mov_edi_imm(b, 64) < 0) return -1; }
    else if (strcmp(fname, "ذاكرة_اقرا")   == 0) { if (jit_emit_mov_edi_imm(b, 32) < 0) return -1; }

    if (jit_emit_call_helper(b, helper) < 0) return -1;
    return jit_emit_load_typed_result(b);
}

/* اطبع / اطبع_سطر */
static int jit_compile_print_typed(const ast_node_t *node, jit_buf_t *b, int *depth)
{
    int is_line = (strcmp(node->function_call.name, "اطبع_سطر") == 0);
    int argc = node->function_call.arg_count;
    if (argc < 1) return -1;

    int    has_color  = 0;
    double color_val  = 0;
    int    value_argc = argc;

    if (argc == 2 &&
        node->function_call.args[1]->type == AST_NUMBER &&
        node->function_call.args[1]->number >= 0x1000)
    {
        has_color  = 1;
        color_val  = node->function_call.args[1]->number;
        value_argc = 1;
    }

    if (jit_emit_call_helper(b, (const void *)jit_rt_print_begin) < 0) return -1;

    if (has_color)
    {
        if (jit_emit_load_double(b, color_val) < 0) return -1;
        if (jit_emit_call_helper(b, (const void *)jit_rt_set_print_color) < 0) return -1;
    }

    for (int i = 0; i < value_argc; i++)
    {
        if (i > 0)
            if (jit_emit_call_helper(b, (const void *)jit_rt_print_space) < 0) return -1;

        if (jit_compile_expr_typed(node->function_call.args[i], b, depth) < 0) return -1;
        if (jit_emit_call_helper(b, (const void *)jit_rt_typed_trace_from_regs) < 0) return -1;
        if (jit_emit_call_helper(b, (const void *)jit_rt_print_value) < 0) return -1;
    }

    const void *flush = is_line
        ? (const void *)jit_rt_print_newline
        : (const void *)jit_rt_print_flush_inline;
    return jit_emit_call_helper(b, flush);
}

/* حرف_النص */
static int jit_compile_text_char_typed(const ast_node_t *node, jit_buf_t *b, int *depth)
{
    if (node->function_call.arg_count != 2) return -1;
    if (jit_compile_expr_typed(node->function_call.args[0], b, depth) < 0) return -1;
    if (*depth >= JIT_MAX_DEPTH) return -1;
    if (jit_emit_store_slot(b, *depth) < 0) return -1;
    (*depth)++;
    if (jit_compile_expr_typed(node->function_call.args[1], b, depth) < 0) return -1;
    if (jit_emit_movsd_xmm1_xmm0(b) < 0) return -1;
    if (jit_emit_load_slot_xmm0(b, *depth - 1) < 0) return -1;
    (*depth)--;
    if (jit_emit_call_helper(b, (const void *)jit_rt_text_char) < 0) return -1;
    return jit_emit_load_typed_result(b);
}

/* عتاد */
static int jit_compile_hw_out_typed(const ast_node_t *node, jit_buf_t *b, int *depth)
{
    if (node->function_call.arg_count != 2) return -1;
    if (jit_compile_expr_typed(node->function_call.args[0], b, depth) < 0) return -1;
    if (*depth >= JIT_MAX_DEPTH) return -1;
    if (jit_emit_store_slot(b, *depth) < 0) return -1;
    (*depth)++;
    if (jit_compile_expr_typed(node->function_call.args[1], b, depth) < 0) return -1;
    if (jit_emit_movsd_xmm1_xmm0(b) < 0) return -1;
    if (jit_emit_load_slot_xmm0(b, *depth - 1) < 0) return -1;
    (*depth)--;
    if (jit_emit_call_helper(b, (const void *)jit_rt_hw_out) < 0) return -1;
    return jit_emit_load_typed_result(b);
}

/* ادخل */
static int jit_compile_input_typed(const ast_node_t *node, jit_buf_t *b)
{
    if (node->function_call.arg_count != 1) return -1;
    const ast_node_t *arg = node->function_call.args[0];
    if (arg->type != AST_VARIABLE) return -1;
    if (jit_emit_name_ref(b, arg->name) < 0) return -1;
    if (jit_emit_call_helper(b, (const void *)jit_rt_input) < 0) return -1;
    return jit_emit_load_typed_result(b);
}

static int jit_emit_mov_rdi_rcx(jit_buf_t *b)
{
    static const uint8_t c[] = { 0x48, 0x89, 0xCF };   /* mov rdi, rcx */
    return jit_emit_bytes(b, c, sizeof(c));
}

static int jit_compile_text_unary_typed(const ast_node_t *node, jit_buf_t *b,
                                        int *depth, const void *helper)
{
    if (node->function_call.arg_count != 1) return -1;
    if (jit_compile_expr_typed(node->function_call.args[0], b, depth) < 0) return -1;
    if (jit_emit_mov_rdi_rcx(b) < 0) return -1;          /* مؤشر النص: rcx → rdi */
    if (jit_emit_call_helper(b, helper) < 0) return -1;
    return jit_emit_load_typed_result(b);
}


/* يقيّم المعاملات ويرصّها في خانات نوعية بترتيب تصاعدي في الذاكرة:
 *   المعامل i  ←  الخانة (base + argc - 1 - i)
 *   slots[i]   =  (rbp - 24*(base+argc)) + 24*i
 * أي أن slots[0] هو المعامل الأول فعلًا. */
static int jit_compile_args_to_slots(const ast_node_t *node, jit_buf_t *b,
                                     int *depth, int *out_base)
{
    int argc = node->function_call.arg_count;
    int base = *depth;
    if (argc < 0 || base + argc > JIT_MAX_DEPTH) return -1;

    /* احجز الخانات حتى لا تستخدمها التعبيرات المتداخلة */
    *depth = base + argc;

    for (int i = 0; i < argc; i++)
    {
        if (jit_compile_expr_typed(node->function_call.args[i], b, depth) < 0)
            { *depth = base; return -1; }
        if (jit_emit_store_typed_slot(b, base + argc - 1 - i) < 0)
            { *depth = base; return -1; }
    }

    *depth = base;
    *out_base = base;
    return 0;
}

/* الموزّع */
static int jit_compile_call_typed(const ast_node_t *node, jit_buf_t *b, int *depth)
{
    const char *fname = node->function_call.name;

    /* ★ هل هو فهرس مصفوفة؟ (المتغير ليس دالة مسجّلة) */
    if (!function_find(fname))
    {
        arp_value_t holder;
        arp_value_init(&holder, ARP_TYPE_UNKNOWN);
        int is_arr = (variable_get_value(fname, &holder) == 0 &&
                      holder.type == ARP_TYPE_ARRAY);
        arp_value_free(&holder);

        if (is_arr)
        {
            int argc = node->function_call.arg_count;
            if (argc < 1 || argc > 8) return -1;

            int base = 0;
            if (jit_compile_args_to_slots(node, b, depth, &base) < 0) return -1;

            /* rdi = name */
            if (jit_emit_name_ref(b, fname) < 0) return -1;

            /* esi = argc */
            if (jit_emit_mov_esi_imm(b, (uint32_t)argc) < 0) return -1;

            /* rdx = &slots[0] (خانات نوعية، ترتيب تصاعدي) */
            if (jit_emit_lea_rdx_rbp_neg(b, (uint32_t)(TYPED_SLOT_SIZE * (base + argc))) < 0)
                return -1;

            if (jit_emit_call_helper(b, (const void *)jit_rt_array_index) < 0)
                return -1;
            return jit_emit_load_typed_result(b);
        }
    }

    if (strcmp(fname, "طول_النص") == 0)
        return jit_compile_text_unary_typed(node, b, depth, (const void *)jit_rt_text_len);
    if (strcmp(fname, "ملف_مؤشر") == 0)
        return jit_compile_text_unary_typed(node, b, depth, (const void *)jit_rt_file_ptr);
    if (strcmp(fname, "اقرا_عتاد") == 0)
        return jit_compile_simple_unary_typed(node, b, depth, (const void *)jit_rt_hw_in);
    if (strcmp(fname, "انتظر") == 0)
        return jit_compile_simple_unary_typed(node, b, depth, (const void *)jit_rt_wait);

    if (strcmp(fname, "اعرض_الشاشة") == 0)
    {
        if (node->function_call.arg_count != 0) return -1;
        if (jit_emit_call_helper(b, (const void *)jit_rt_present) < 0) return -1;
        return jit_emit_load_typed_result(b);
    }
    if (strcmp(fname, "شغل") == 0)
        return jit_compile_name_unary_typed(node, b, (const void *)jit_rt_app);
    if (strcmp(fname, "ذاكرة_خصص_صفحة") == 0)
    {
        if (node->function_call.arg_count != 0) return -1;
        if (jit_emit_call_helper(b, (const void *)jit_rt_mem_alloc_page) < 0) return -1;
        return jit_emit_load_typed_result(b);
    }
    if (strcmp(fname, "حرف_النص") == 0)
        return jit_compile_text_char_typed(node, b, depth);
    if (strcmp(fname, "عتاد") == 0)
        return jit_compile_hw_out_typed(node, b, depth);
    if (strcmp(fname, "ادخل") == 0)
        return jit_compile_input_typed(node, b);
    {
        int r = jit_compile_memory_fn_typed(node, b, depth);
        if (r >= 0) return r;
    }
    if (strcmp(fname, "اطبع") == 0 || strcmp(fname, "اطبع_سطر") == 0)
        return jit_compile_print_typed(node, b, depth);

    /* دالة ARP مسجّلة */
    if (function_find(fname))
    {
        int argc = node->function_call.arg_count;
        if (argc < 0 || argc > JIT_MAX_ARGC) return -1;

        int base = 0;
        if (jit_compile_args_to_slots(node, b, depth, &base) < 0) return -1;

        if (jit_emit_name_ref(b, fname) < 0) return -1;                     /* rdi = الاسم   */
        if (jit_emit_mov_esi_imm(b, (uint32_t)argc) < 0) return -1;         /* esi = العدد   */
        if (jit_emit_lea_rdx_rbp_neg(b,
                (uint32_t)(TYPED_SLOT_SIZE * (base + argc))) < 0)           /* rdx = slots   */
            return -1;

        if (jit_emit_call_helper(b, (const void *)jit_rt_typed_call_function) < 0)
            return -1;
        return jit_emit_load_typed_result(b);
    }

    return -1;
}

static int jit_compile_expr_typed(const ast_node_t *node, jit_buf_t *b, int *depth)
{
    if (!node || !b || !depth) return -1;
    switch (node->type)
    {
        case AST_NUMBER:   return jit_compile_number_typed(node, b, depth);
        case AST_STRING:   return jit_compile_string_typed(node, b, depth);
        case AST_VARIABLE: return jit_compile_variable_typed(node, b, depth);
        case AST_ASSIGN:   return jit_compile_assign_typed(node, b, depth);
        case AST_CMP_EQ: case AST_CMP_NEQ:
        case AST_CMP_LT: case AST_CMP_LTE:
        case AST_CMP_GT: case AST_CMP_GTE:
            return jit_compile_compare_typed(node, b, depth);
        case AST_ADD: return jit_compile_add_typed(node, b, depth);
        case AST_SUB: return jit_compile_binop_typed(node, b, depth, (const void *)jit_rt_typed_sub);
        case AST_MUL: return jit_compile_binop_typed(node, b, depth, (const void *)jit_rt_typed_mul);
        case AST_DIV: return jit_compile_binop_typed(node, b, depth, (const void *)jit_rt_typed_div);
        case AST_MOD: return jit_compile_binop_typed(node, b, depth, (const void *)jit_rt_typed_mod);
        case AST_SHL: return jit_compile_binop_typed(node, b, depth, (const void *)jit_rt_typed_shl);
        case AST_SHR: return jit_compile_binop_typed(node, b, depth, (const void *)jit_rt_typed_shr);
        case AST_BIT_AND: return jit_compile_binop_typed(node, b, depth, (const void *)jit_rt_typed_bit_and);
        case AST_BIT_OR:  return jit_compile_binop_typed(node, b, depth, (const void *)jit_rt_typed_bit_or);
        case AST_BIT_XOR: return jit_compile_binop_typed(node, b, depth, (const void *)jit_rt_typed_bit_xor);
        case AST_AND: return jit_compile_binop_typed(node, b, depth, (const void *)jit_rt_typed_and);
        case AST_OR:  return jit_compile_binop_typed(node, b, depth, (const void *)jit_rt_typed_or);

        case AST_NEG: return jit_compile_neg_typed(node, b, depth);
        case AST_FUNCTION_CALL:
            return jit_compile_call_typed(node, b, depth);
        default: return -1;
    }
}

/* =========================================================
 * إنهاء مراجع الأسماء
 * ========================================================= */

static int jit_finalize_names(jit_buf_t *b)
{
    if (b->ref_count == 0) return 0;
    size_t *offs = (size_t *)memory_api.alloc(b->ref_count * sizeof(size_t));
    if (!offs) return -1;
    for (int i = 0; i < b->ref_count; i++)
    {
        offs[i] = b->size;
        if (jit_emit_bytes(b, (const uint8_t *)b->refs[i].name,
                           b->refs[i].name_len + 1) < 0)
        {
            memory_api.free(offs);
            return -1;
        }
    }
    for (int i = 0; i < b->ref_count; i++)
    {
        jit_name_ref_t *r = &b->refs[i];
        int64_t d = (int64_t)offs[i] - (int64_t)(r->disp_offset + 4);
        if (d < -2147483648LL || d > 2147483647LL)
        {
            memory_api.free(offs);
            return -1;
        }
        int32_t d32 = (int32_t)d;
        b->bytes[r->disp_offset + 0] = (uint8_t)((d32 >>  0) & 0xFF);
        b->bytes[r->disp_offset + 1] = (uint8_t)((d32 >>  8) & 0xFF);
        b->bytes[r->disp_offset + 2] = (uint8_t)((d32 >> 16) & 0xFF);
        b->bytes[r->disp_offset + 3] = (uint8_t)((d32 >> 24) & 0xFF);
    }
    memory_api.free(offs);
    return 0;
}

/* =========================================================
 * مساعدات التحكم بالكتل
 * ========================================================= */

static const char *find_call_close(const char *open_paren)
{
    if (!open_paren || *open_paren != '(') return NULL;
    int d = 1, istr = 0;
    const char *q = open_paren + 1;
    while (*q)
    {
        if (*q == '"') istr = !istr;
        if (!istr)
        {
            if (*q == '(') d++;
            else if (*q == ')') { d--; if (d == 0) return q; }
        }
        q++;
    }
    return NULL;
}

static int find_block_end(const char **lines, int start, int total,
                          int (*is_start)(const char *),
                          int (*is_end)(const char *),
                          int *else_index)
{
    int depth = 1;
    if (else_index) *else_index = -1;
    for (int j = start + 1; j < total; j++)
    {
        const char *b = skip_spaces(lines[j]);
        if (is_start(b)) { depth++; continue; }
        if (is_end(b))
        {
            depth--;
            if (depth == 0) return j;
            continue;
        }
        if (else_index && depth == 1 && condition_is_else(b))
            if (*else_index < 0) *else_index = j;
    }
    return -1;
}

static int mc_return_push(size_t disp)
{
    if (g_mc_return_patches.count >= g_mc_return_patches.capacity)
    {
        int nc = g_mc_return_patches.capacity ? g_mc_return_patches.capacity * 2 : 8;
        mc_reloc_t *na = (mc_reloc_t *)memory_api.alloc(nc * sizeof(mc_reloc_t));
        if (!na) return -1;
        for (int i = 0; i < g_mc_return_patches.count; i++)
            na[i] = g_mc_return_patches.items[i];
        if (g_mc_return_patches.items)
            memory_api.free(g_mc_return_patches.items);
        g_mc_return_patches.items    = na;
        g_mc_return_patches.capacity = nc;
    }
    g_mc_return_patches.items[g_mc_return_patches.count++].disp_offset = disp;
    return 0;
}

static void mc_return_clear(void)
{
    if (g_mc_return_patches.items) memory_api.free(g_mc_return_patches.items);
    g_mc_return_patches.items    = NULL;
    g_mc_return_patches.count    = 0;
    g_mc_return_patches.capacity = 0;
}

static int mc_emit_jmp(jit_buf_t *b, size_t *out_disp)
{
    if (jit_emit8(b, 0xE9) < 0) return -1;
    *out_disp = b->size;
    return jit_emit32(b, 0);
}

static int mc_emit_je(jit_buf_t *b, size_t *out_disp)
{
    if (jit_emit8(b, 0x0F) < 0) return -1;
    if (jit_emit8(b, 0x84) < 0) return -1;
    *out_disp = b->size;
    return jit_emit32(b, 0);
}

static int mc_patch_rel32(jit_buf_t *b, size_t disp_off, size_t target)
{
    int64_t d = (int64_t)target - (int64_t)(disp_off + 4);
    if (d < -2147483648LL || d > 2147483647LL) return -1;
    int32_t d32 = (int32_t)d;
    b->bytes[disp_off + 0] = (uint8_t)((d32 >>  0) & 0xFF);
    b->bytes[disp_off + 1] = (uint8_t)((d32 >>  8) & 0xFF);
    b->bytes[disp_off + 2] = (uint8_t)((d32 >> 16) & 0xFF);
    b->bytes[disp_off + 3] = (uint8_t)((d32 >> 24) & 0xFF);
    return 0;
}

static int mc_emit_jump_if_typed_false(jit_buf_t *b, size_t *out_disp)
{
    static const uint8_t pre[] = {
        0x66, 0x0F, 0x57, 0xC9,
        0x66, 0x0F, 0x2E, 0xC1
    };
    if (jit_emit_bytes(b, pre, sizeof(pre)) < 0) return -1;
    return mc_emit_je(b, out_disp);
}

static char *mc_extract_if_cond(const char *line)
{
    const char *p = skip_spaces(line);
    if (!starts_with(p, "اذا(")) return NULL;
    p = skip_spaces(skip_prefix(p, "اذا("));
    int d = 1, istr = 0;
    const char *q = p;
    while (*q)
    {
        if (*q == '"') istr = !istr;
        if (!istr)
        {
            if (*q == '(') d++;
            else if (*q == ')') { d--; if (d == 0) break; }
        }
        q++;
    }
    if (*q != ')') return NULL;
    return arp_strndup(p, q - p);
}

static int mc_line_is_jit_expr(const char *line, ast_node_t **root_out)
{
    *root_out = NULL;
    int err = 0;
    ast_node_t *root = math_parse_ast(line, &err);
    if (err || !root) { ast_free(root); return 0; }
    if (!machine_ast_is_jit_supported(root)) { ast_free(root); return 0; }
    *root_out = root;
    return 1;
}

static ast_node_t *mc_parse_condition(const char *cond)
{
    if (!cond) return NULL;
    int len = (int)strlen(cond);
    int eq_pos = -1;
    int d = 0, istr = 0;
    for (int k = 0; k < len; k++)
    {
        char c = cond[k];
        if (c == '"') { istr = !istr; continue; }
        if (istr) continue;
        if (c == '(') { d++; continue; }
        if (c == ')') { if (d > 0) d--; continue; }
        if (d != 0) continue;
        if (c == '=')
        {
            if (k > 0 && (cond[k-1] == '=' || cond[k-1] == '!' ||
                          cond[k-1] == '>' || cond[k-1] == '<'))
                continue;
            if (k + 1 < len && (cond[k+1] == '=' || cond[k+1] == '!'))
                continue;
            eq_pos = k;
            break;
        }
    }
    if (eq_pos <= 0)
    {
        int err = 0;
        ast_node_t *root = math_parse_ast(cond, &err);
        if (err) { ast_free(root); return NULL; }
        return root;
    }
    int left_len = eq_pos;
    while (left_len > 0 && (cond[left_len-1] == ' ' || cond[left_len-1] == '\t'))
        left_len--;
    const char *rs = cond + eq_pos + 1;
    while (*rs == ' ' || *rs == '\t') rs++;
    int right_len = (int)strlen(rs);
    while (right_len > 0 && (rs[right_len-1] == ' ' || rs[right_len-1] == '\t'))
        right_len--;
    if (left_len <= 0 || right_len <= 0) return NULL;
    char *lhs_str = arp_strndup(cond, left_len);
    char *rhs_str = arp_strndup(rs, right_len);
    if (!lhs_str || !rhs_str)
    {
        if (lhs_str) memory_api.free(lhs_str);
        if (rhs_str) memory_api.free(rhs_str);
        return NULL;
    }
    int eL = 0, eR = 0;
    ast_node_t *lhs_root = math_parse_ast(lhs_str, &eL);
    ast_node_t *rhs_root = math_parse_ast(rhs_str, &eR);
    memory_api.free(lhs_str);
    memory_api.free(rhs_str);
    if (eL || !lhs_root || eR || !rhs_root)
    {
        ast_free(lhs_root);
        ast_free(rhs_root);
        return NULL;
    }
    ast_node_t *result = ast_binary(AST_CMP_EQ, lhs_root, rhs_root);
    if (!result) { ast_free(lhs_root); ast_free(rhs_root); return NULL; }
    return result;
}

/* =========================================================
 * ترجمة الأسطر الخاصة
 * ========================================================= */

static int mc_try_compile_raw_path(const char *p, jit_buf_t *b)
{
    const char *cmd = NULL;
    const void *helper = NULL;
    if (starts_with(p, "شغل(")) { cmd = "شغل"; helper = (const void *)jit_rt_app; }
    if (!cmd) return 0;
    const char *open = p + strlen(cmd);
    const char *close = find_call_close(open);
    if (!close) return -1;
    if (*skip_spaces(close + 1) != '\0') return -1;
    const char *s = open + 1;
    while (s < close && (*s == ' ' || *s == '\t')) s++;
    const char *e = close;
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) e--;
    int plen = e - s;
    if (plen <= 0) return -1;
    char *path = arp_strndup(s, plen);
    if (!path) return -1;
    if (jit_emit_name_ref(b, path) < 0) { memory_api.free(path); return -1; }
    memory_api.free(path);
    if (jit_emit_call_helper(b, helper) < 0) return -1;
    return 1;
}

static int mc_compile_if(const char **lines, int i, int total, jit_buf_t *b, int *out_next)
{
    int else_idx = -1;
    int end_idx = find_block_end(lines, i, total,
                                 condition_is_start, condition_is_end,
                                 &else_idx);
    if (end_idx < 0) return -1;
    char *cond = mc_extract_if_cond(lines[i]);
    if (!cond) return -1;
    int depth = 0;
    const char *trim = skip_spaces(cond);
    if (starts_with(trim, "لوحة_المفاتيح("))
    {
        memory_api.free(cond);
        if (jit_emit_name_ref(b, lines[i]) < 0) return -1;
        if (jit_emit_call_helper(b, (const void *)jit_rt_keyboard_cond) < 0) return -1;
    }
    else
    {
        ast_node_t *croot = mc_parse_condition(cond);
        memory_api.free(cond);
        if (!croot || !machine_ast_is_jit_supported(croot)) { ast_free(croot); return -1; }
        if (jit_compile_expr_typed(croot, b, &depth) < 0) { ast_free(croot); return -1; }
        ast_free(croot);
    }
    size_t jf_disp;
    if (mc_emit_jump_if_typed_false(b, &jf_disp) < 0) return -1;
    int then_start = i + 1;
    int then_end   = (else_idx >= 0) ? else_idx : end_idx;
    if (mc_compile_block(&lines[then_start], then_end - then_start, b) < 0) return -1;
    if (else_idx >= 0)
    {
        size_t jmp_end_disp;
        if (mc_emit_jmp(b, &jmp_end_disp) < 0) return -1;
        if (mc_patch_rel32(b, jf_disp, b->size) < 0) return -1;
        int else_start = else_idx + 1;
        if (mc_compile_block(&lines[else_start], end_idx - else_start, b) < 0) return -1;
        if (mc_patch_rel32(b, jmp_end_disp, b->size) < 0) return -1;
    }
    else
    {
        if (mc_patch_rel32(b, jf_disp, b->size) < 0) return -1;
    }
    *out_next = end_idx + 1;
    return 0;
}

static int mc_compile_loop(const char **lines, int i, int total, jit_buf_t *b, int *out_next)
{
    int end_idx = find_block_end(lines, i, total,
                                 loop_is_start, loop_is_end, NULL);
    if (end_idx < 0) return -1;
    if (g_mc_loop_depth >= MC_MAX_LOOP_DEPTH) return -1;
    mc_loop_t ctx;
    ctx.break_count    = 0;
    ctx.continue_count = 0;
    ctx.loop_start     = b->size;
    g_mc_loop_stack[g_mc_loop_depth++] = &ctx;
    int body_start = i + 1;
    if (mc_compile_block(&lines[body_start], end_idx - body_start, b) < 0)
    {
        g_mc_loop_depth--;
        return -1;
    }
    g_mc_loop_depth--;
    size_t back_disp;
    if (mc_emit_jmp(b, &back_disp) < 0) return -1;
    if (mc_patch_rel32(b, back_disp, ctx.loop_start) < 0) return -1;
    size_t end_target = b->size;
    for (int k = 0; k < ctx.break_count; k++)
        if (mc_patch_rel32(b, ctx.breaks[k].disp_offset, end_target) < 0) return -1;
    for (int k = 0; k < ctx.continue_count; k++)
        if (mc_patch_rel32(b, ctx.continues[k].disp_offset, ctx.loop_start) < 0) return -1;
    *out_next = end_idx + 1;
    return 0;
}

static int mc_compile_loop_control(const char *p, jit_buf_t *b)
{
    if (g_mc_loop_depth <= 0) return -1;
    mc_loop_t *ctx = g_mc_loop_stack[g_mc_loop_depth - 1];
    size_t disp;
    if (mc_emit_jmp(b, &disp) < 0) return -1;
    if (starts_with(p, "كسر("))
    {
        if (ctx->break_count >= 32) return -1;
        ctx->breaks[ctx->break_count++].disp_offset = disp;
    }
    else
    {
        if (ctx->continue_count >= 32) return -1;
        ctx->continues[ctx->continue_count++].disp_offset = disp;
    }
    return 0;
}

static int mc_compile_return_typed(const char *p, jit_buf_t *b)
{
    const char *open = p + strlen("ارجع(");
    int d = 1, istr = 0;
    const char *c = open;
    while (*c)
    {
        if (*c == '"') istr = !istr;
        if (!istr)
        {
            if (*c == '(') d++;
            else if (*c == ')') { d--; if (d == 0) break; }
        }
        c++;
    }
    if (*c != ')') return -1;
    int elen = c - open;
    if (elen == 0)
    {
        size_t disp;
        if (mc_emit_jmp(b, &disp) < 0) return -1;
        return mc_return_push(disp);
    }
    char *expr = arp_strndup(open, elen);
    if (!expr) return -1;
    int err = 0;
    ast_node_t *rroot = math_parse_ast(expr, &err);
    memory_api.free(expr);
    if (err || !rroot) { ast_free(rroot); return -1; }
    if (!machine_ast_is_jit_supported(rroot)) { ast_free(rroot); return -1; }
    int depth = 0;
    if (jit_compile_expr_typed(rroot, b, &depth) < 0) { ast_free(rroot); return -1; }
    ast_free(rroot);
    /* ثبّت القيمة في الأثر العام قبل القفز إلى الخاتمة */
    if (jit_emit_call_helper(b, (const void *)jit_rt_typed_return_from_regs) < 0) return -1;
    size_t disp;
    if (mc_emit_jmp(b, &disp) < 0) return -1;
    return mc_return_push(disp);
}

static int mc_compile_struct_instance(const char *p, jit_buf_t *b)
{
    int d = 0, istr = 0;
    const char *colon = NULL;
    int has_assign = 0;
    for (const char *q = p; *q; q++)
    {
        if (*q == '"') { istr = !istr; continue; }
        if (istr) continue;
        if (*q == '(') { d++; continue; }
        if (*q == ')') { d--; continue; }
        if (d != 0) continue;
        if (*q == '=') { has_assign = 1; break; }
        if (*q == ':') { colon = q; break; }
    }
    if (!colon || colon == p || has_assign) return 0;
    int tlen = colon - p;
    while (tlen > 0 && (p[tlen-1] == ' ' || p[tlen-1] == '\t')) tlen--;
    if (tlen <= 0) return 0;
    char *tname = arp_strndup(p, tlen);
    if (!tname) return -1;
    if (!struct_exists(tname)) { memory_api.free(tname); return 0; }
    const char *inst = skip_spaces(colon + 1);
    int ilen = 0;
    while (inst[ilen] && inst[ilen] != ' ' && inst[ilen] != '\t') ilen++;
    const char *rest = skip_spaces(inst + ilen);
    if (ilen <= 0 || *rest != '\0') { memory_api.free(tname); return 0; }
    char *iname = arp_strndup(inst, ilen);
    if (!iname) { memory_api.free(tname); return -1; }
    int ok = 1;
    if (jit_emit_name_ref(b, tname) < 0) ok = 0;
    else if (jit_emit_name_ref_rsi(b, iname) < 0) ok = 0;
    else if (jit_emit_call_helper(b, (const void *)jit_rt_create_struct) < 0) ok = 0;
    memory_api.free(tname);
    memory_api.free(iname);
    return ok ? 1 : -1;
}

static int mc_compile_assign_line_typed(const char *p, jit_buf_t *b)
{
    int d = 0, istr = 0;
    const char *eq = NULL;
    for (const char *q = p; *q; q++)
    {
        if (*q == '"') { istr = !istr; continue; }
        if (istr) continue;
        if (*q == '(') { d++; continue; }
        if (*q == ')') { d--; continue; }
        if (d != 0) continue;
        if (*q == '=')
        {
            if (q > p && (q[-1] == '=' || q[-1] == '!' ||
                          q[-1] == '>' || q[-1] == '<')) continue;
            if (q[1] == '=' || q[1] == '!') continue;
            eq = q;
            break;
        }
    }
    if (!eq) return 0;
    int nlen = eq - p;
    while (nlen > 0 && (p[nlen-1] == ' ' || p[nlen-1] == '\t')) nlen--;
    if (nlen <= 0) return -1;
    char *name = arp_strndup(p, nlen);
    if (!name) return -1;
    const char *rhs = skip_spaces(eq + 1);
    if (*rhs == '\0') { memory_api.free(name); return -1; }
    int err = 0;
    ast_node_t *rhs_root = math_parse_ast(rhs, &err);
    if (err || !rhs_root || !machine_ast_is_jit_supported(rhs_root))
    {
        ast_free(rhs_root);
        memory_api.free(name);
        return -1;
    }
    ast_node_t *assign = ast_assignment(name, rhs_root);
    memory_api.free(name);
    if (!assign) { ast_free(rhs_root); return -1; }
    int depth = 0;
    int rc = jit_compile_expr_typed(assign, b, &depth);
    ast_free(assign);
    return (rc < 0) ? -1 : 1;
}

static int mc_compile_simple_expr_line_typed(const char *line, jit_buf_t *b)
{
    ast_node_t *root = NULL;
    if (!mc_line_is_jit_expr(line, &root)) return -1;
    int depth = 0;
    if (jit_compile_expr_typed(root, b, &depth) < 0) { ast_free(root); return -1; }
    ast_free(root);
    return 0;
}

static int mc_compile_declaration_typed(const char *p, jit_buf_t *b)
{
    arp_type_t  decl_type;
    char       *decl_name = NULL;
    const char *rhs       = NULL;
    if (!declaration_is_start(p, &decl_type, &decl_name, &rhs))
        return 0;
    int err = 0;
    ast_node_t *rhs_root = math_parse_ast(rhs, &err);
    if (err || !rhs_root)
    {
        ast_free(rhs_root);
        declaration_free(decl_name);
        return -1;
    }
    if (!machine_ast_is_jit_supported(rhs_root))
    {
        ast_free(rhs_root);
        declaration_free(decl_name);
        return -1;
    }
    int depth = 0;
    if (jit_compile_expr_typed(rhs_root, b, &depth) < 0)
    {
        ast_free(rhs_root);
        declaration_free(decl_name);
        return -1;
    }
    ast_free(rhs_root);

    if (jit_emit_store_typed_slot(b, 0) < 0) { declaration_free(decl_name); return -1; }
    if (jit_emit_load_typed_slot(b, 0) < 0) { declaration_free(decl_name); return -1; }
    if (jit_emit_call_helper(b, (const void *)jit_rt_typed_trace_from_regs) < 0)
    {
        declaration_free(decl_name);
        return -1;
    }
    if (jit_emit_name_ref(b, decl_name) < 0) { declaration_free(decl_name); return -1; }
    if (jit_emit_mov_esi_imm(b, (uint32_t)decl_type) < 0) { declaration_free(decl_name); return -1; }
    if (jit_emit_call_helper(b, (const void *)jit_rt_typed_declare_variable) < 0)
    {
        declaration_free(decl_name);
        return -1;
    }
    if (jit_emit_load_typed_result(b) < 0) { declaration_free(decl_name); return -1; }
    declaration_free(decl_name);
    return 1;
}

/* ★ تعبئة g_array_values/g_array_dims/g_array_type من AST_ARRAY_LIT
 *   (recursive) ثم توليد استدعاء jit_rt_declare_array. */
static int jit_fill_array_literal(const ast_node_t *root, arp_type_t elem_type,
                                  int depth, int *flat_idx)
{
    if (!root || root->type != AST_ARRAY_LIT) return -1;

    for (int i = 0; i < root->array_lit.element_count; i++)
    {
        const ast_node_t *e = root->array_lit.elements[i];
        if (e->type == AST_ARRAY_LIT)
        {
            if (jit_fill_array_literal(e, elem_type, depth + 1, flat_idx) < 0)
                return -1;
        }
        else
        {
            /* عنصر عددي — قيّمه */
            arp_value_t v;
            arp_value_init(&v, ARP_TYPE_UNKNOWN);
            int err = 0;
            if (ast_evaluate_typed((ast_node_t *)e, &v, &err) < 0 || err)
            {
                arp_value_free(&v);
                return -1;
            }

            /* coercion إلى elem_type */
            arp_value_t coerced;
            arp_value_init(&coerced, elem_type);
            if (arp_value_coerce(&v, &coerced) < 0)
            {
                arp_value_free(&v);
                arp_value_free(&coerced);
                return -1;
            }
            arp_value_free(&v);

            if (*flat_idx >= JIT_ARRAY_MAX_VALUES)
            {
                arp_value_free(&coerced);
                return -1;
            }

            arp_value_free(&g_array_values[*flat_idx]);
            g_array_values[*flat_idx] = coerced;
            (*flat_idx)++;
        }
    }
    return 0;
}

static int jit_compile_array_decl(const char *name, arp_type_t elem_type,
                                  int ndims, const int *dims,
                                  const ast_node_t *lit, jit_buf_t *b)
{
    if (!name || !dims || ndims <= 0 || !lit) return -1;
    if (lit->type != AST_ARRAY_LIT) return -1;

    /* املأ القيم والأبعاد في globals */
    g_array_type  = elem_type;
    g_array_ndims = ndims;
    for (int i = 0; i < ndims; i++) g_array_dims[i] = dims[i];

    /* صفّر القيم القديمة */
    for (int i = 0; i < g_array_value_count; i++)
        arp_value_free(&g_array_values[i]);
    g_array_value_count = 0;

    int flat_idx = 0;
    if (jit_fill_array_literal(lit, elem_type, 0, &flat_idx) < 0) return -1;
    g_array_value_count = flat_idx;

    /* تحقق من العدد المتوقع */
    int expected = 1;
    for (int i = 0; i < ndims; i++) expected *= dims[i];
    if (expected != flat_idx) return -1;

    /* توليد: jit_rt_declare_array(name) */
    if (jit_emit_name_ref(b, name) < 0) return -1;
    if (jit_emit_call_helper(b, (const void *)jit_rt_declare_array) < 0) return -1;
    return jit_emit_load_typed_result(b);
}

/* =========================================================
 * ترجمة الكتلة الكاملة
 * ========================================================= */

static int mc_compile_block(const char **lines, int line_count, jit_buf_t *b)
{
    if (!lines || line_count < 0 || !b) return -1;
    int i = 0;
    while (i < line_count)
    {
        const char *line = lines[i];
        const char *p = skip_spaces(line);
        if (*p == '\0') { i++; continue; }

        {
            int r = mc_try_compile_raw_path(p, b);
            if (r == 1) { i++; continue; }
            if (r < 0) return -1;
        }
        if (condition_is_start(p))
        {
            int next;
            if (mc_compile_if(lines, i, line_count, b, &next) < 0) return -1;
            i = next;
            continue;
        }
        if (loop_is_start(p))
        {
            int next;
            if (mc_compile_loop(lines, i, line_count, b, &next) < 0) return -1;
            i = next;
            continue;
        }
        if (starts_with(p, "كسر(") || starts_with(p, "استمر(") || starts_with(p, "اعد("))
        {
            if (mc_compile_loop_control(p, b) < 0) return -1;
            i++;
            continue;
        }
        if (starts_with(p, "ارجع("))
        {
            if (mc_compile_return_typed(p, b) < 0) return -1;
            i++;
            continue;
        }
        {
            int r = mc_compile_struct_instance(p, b);
            if (r == 1) { i++; continue; }
            if (r < 0) return -1;
        }
        /* ★ تصريح مصفوفة: صحيح مدونة(4) = {...} */
        {
            arp_type_t  arr_type;
            char       *arr_name = NULL;
            int         arr_dims[8];
            int         arr_ndims = 0;
            const char *arr_rhs = NULL;

            if (declaration_is_array_start(p, &arr_type, &arr_name,
                                            arr_dims, &arr_ndims, &arr_rhs))
            {
                /* حلّل { ... } إلى AST_ARRAY_LIT */
                int err = 0;
                ast_node_t *lit = math_parse_ast(arr_rhs, &err);
                if (err || !lit || lit->type != AST_ARRAY_LIT)
                {
                    ast_free(lit);
                    declaration_free(arr_name);
                    return -1;
                }

                int rc = jit_compile_array_decl(arr_name, arr_type,
                                                arr_ndims, arr_dims,
                                                lit, b);
                ast_free(lit);
                declaration_free(arr_name);
                if (rc < 0) return -1;
                i++;
                continue;
            }
        }

        if (condition_is_else(p) || condition_is_end(p) || loop_is_end(p))
            return -1;
        {
            int r = mc_compile_declaration_typed(p, b);
            if (r == 1) { i++; continue; }
            if (r < 0) return -1;
        }
        {
            int r = mc_compile_assign_line_typed(p, b);
            if (r == 1) { i++; continue; }
            if (r < 0) return -1;
        }
        if (mc_compile_simple_expr_line_typed(line, b) < 0) return -1;
        i++;
    }
    return 0;
}

/* =========================================================
 * نقطة الدخول — ترجمة جسم دالة
 * ========================================================= */

static int jit_compile_function_impl(const char **lines, int line_count,
                                     void **out_code, size_t *out_size,
                                     int aot,
                                     jit_reloc_t **out_relocs,
                                     uint32_t *out_reloc_count)
{
    if (!lines || line_count <= 0 || !out_code || !out_size) return -1;
    *out_code = NULL;
    *out_size = 0;
    if (out_relocs)       *out_relocs = NULL;
    if (out_reloc_count)  *out_reloc_count = 0;

    jit_buf_t b;
    jit_buf_init(&b);
    b.aot = aot;

    mc_return_clear();

    static const uint8_t prologue[] = {
        0x55,
        0x48, 0x89, 0xE5,
        0x48, 0x81, 0xEC,
        (uint8_t)( JIT_FRAME_BYTES        & 0xFF),
        (uint8_t)((JIT_FRAME_BYTES >>  8) & 0xFF),
        (uint8_t)((JIT_FRAME_BYTES >> 16) & 0xFF),
        (uint8_t)((JIT_FRAME_BYTES >> 24) & 0xFF)
    };
    if (jit_emit_bytes(&b, prologue, sizeof(prologue)) < 0) goto fail;

    g_mc_loop_depth = 0;

    if (mc_compile_block(lines, line_count, &b) < 0) goto fail;

    size_t epilogue_start = b.size;

    static const uint8_t epilogue[] = { 0x48, 0x89, 0xEC, 0x5D, 0xC3 };
    if (jit_emit_bytes(&b, epilogue, sizeof(epilogue)) < 0) goto fail;

    for (int k = 0; k < g_mc_return_patches.count; k++)
        if (mc_patch_rel32(&b, g_mc_return_patches.items[k].disp_offset,
                           epilogue_start) < 0)
            goto fail;

    if (jit_finalize_names(&b) < 0) goto fail;

    void *exec = memory_api.alloc(b.size);
    if (!exec) goto fail;
    memcpy(exec, b.bytes, b.size);

    if (aot && out_relocs && out_reloc_count && b.reloc_count > 0)
    {
        jit_reloc_t *rc = (jit_reloc_t *)memory_api.alloc(
                              b.reloc_count * sizeof(jit_reloc_t));
        if (!rc) { memory_api.free(exec); goto fail; }
        memcpy(rc, b.relocs, b.reloc_count * sizeof(jit_reloc_t));
        *out_relocs      = rc;
        *out_reloc_count = b.reloc_count;
    }

    *out_code = exec;
    *out_size = b.size;

    mc_return_clear();
    jit_buf_free(&b);
    return 0;

fail:
    mc_return_clear();
    jit_buf_free(&b);
    return -1;
}

int machine_jit_compile_function(const char **lines, int line_count,
                                 void **out_code, size_t *out_size)
{
    return jit_compile_function_impl(lines, line_count, out_code, out_size,
                                     0, NULL, NULL);
}

int machine_jit_compile_function_reloc(const char **lines, int line_count,
                                       void **out_code, size_t *out_size,
                                       jit_reloc_t **out_relocs,
                                       uint32_t *out_reloc_count)
{
    return jit_compile_function_impl(lines, line_count, out_code, out_size,
                                     1, out_relocs, out_reloc_count);
}

/* =========================================================
 * تنفيذ الكود
 * ========================================================= */

int machine_jit_execute_typed(void *code, jit_result_t *out)
{
    if (!code || !out) return -1;

    out->type = ARP_TYPE_UNKNOWN;
    out->numeric = 0.0;
    out->text = NULL;

    jit_typed_trace_clear();
    jit_typed_trace_left_clear();

    double (*fn)(void) = (double (*)(void))code;
    double r = fn();

    if (g_jit_typed_trace.type != ARP_TYPE_UNKNOWN)
    {
        out->type = g_jit_typed_trace.type;
        out->numeric = g_jit_typed_trace.numeric;
        out->text = g_jit_typed_trace.text;
        g_jit_typed_trace.text = NULL;
        g_jit_typed_trace.type = ARP_TYPE_UNKNOWN;
    }
    else
    {
        out->type = ARP_TYPE_DOUBLE;
        out->numeric = r;
    }

    return 0;
}

void jit_result_free(jit_result_t *r)
{
    if (!r) return;
    if (r->type == ARP_TYPE_TEXT && r->text)
    {
        memory_api.free(r->text);
        r->text = NULL;
    }
    r->type = ARP_TYPE_UNKNOWN;
    r->numeric = 0.0;
}

int machine_jit_take_text_result(char **out_text)
{
    if (out_text) *out_text = NULL;

    if (g_jit_typed_trace.type == ARP_TYPE_TEXT && g_jit_typed_trace.text)
    {
        if (out_text) *out_text = g_jit_typed_trace.text;
        g_jit_typed_trace.text = NULL;
        g_jit_typed_trace.type = ARP_TYPE_UNKNOWN;
        return 1;
    }
    return 0;
}
