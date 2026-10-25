#include "مجمع_الترجمة.h"
#include "المحلل.h"
#include "../مفاهيم_اللغة/الدوال.h"
#include "../مفاهيم_اللغة/المتغيرات.h"
#include "../مفاهيم_اللغة/الهياكل.h"
#include "../مفاهيم_اللغة/التعليقات.h"
#include "../مفاهيم_اللغة/مكتبات_اللغة.h"
#include "مترجم_اللغة.h"
#include "../ادوات/ادوات.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "القلب/الوحدات/محمل_التطبيقات/سياق_اللغة.h"
#include "القلب/الوحدات/محمل_التطبيقات/قيم_الملف.h"
#include "ادوات/شاشة_التطوير/شاشة_التطوير.h"
#include "../لغة.h"

#define AOT_ALIGN(x, a)  (((x) + ((a)-1)) & ~((a)-1))

/* =========================================================
 * طباعة تشخيصية
 * ========================================================= */
static void aot_diag(const char *msg)
{
    //development_screen_print("[AOT] ");
    //development_screen_print(msg);
    //development_screen_println("");
    development_screen_update();
}

static void aot_diag_int(const char *label, int value)
{
    //development_screen_print("[AOT] ");
    //development_screen_print(label);
    //development_screen_print("=");

    char buf[24];
    int i = 0;
    if (value < 0) {
        //development_screen_print("-");
        value = -value;
    }
    if (value == 0) buf[i++] = '0';
    else {
        char tmp[24];
        int n = 0;
        while (value > 0) { tmp[n++] = '0' + (value % 10); value /= 10; }
        while (n > 0) buf[i++] = tmp[--n];
    }
    buf[i] = '\0';
    //development_screen_print(buf);
    //development_screen_println("");
    development_screen_update();
}

/* =========================================================
 * كود x86_64 فارغ
 * ========================================================= */
static const uint8_t g_empty_fn_code[] = {
    0xB8, 0x00, 0x00, 0x00, 0x00,   /* mov eax, 0 */
    0xC3                             /* ret        */
};

/* =========================================================
 * بناء الملف التنفيذي
 * ========================================================= */

typedef struct {
    uint8_t *data;
    size_t   size;
    size_t   capacity;
} aot_buffer_t;

static int aot_buf_init(aot_buffer_t *b, size_t initial)
{
    b->data = (uint8_t *)memory_api.alloc(initial);
    if (!b->data) return -1;
    b->size = 0;
    b->capacity = initial;
    return 0;
}

static int aot_buf_grow(aot_buffer_t *b, size_t extra)
{
    if (b->size + extra <= b->capacity) return 0;

    size_t nc = b->capacity ? b->capacity : 4096;
    while (nc < b->size + extra) nc *= 2;

    uint8_t *nd = (uint8_t *)memory_api.alloc(nc);
    if (!nd) return -1;

    if (b->data) {
        memcpy(nd, b->data, b->size);
        memory_api.free(b->data);
    }
    b->data = nd;
    b->capacity = nc;
    return 0;
}

static int aot_buf_append(aot_buffer_t *b, const void *data, size_t n)
{
    if (aot_buf_grow(b, n) < 0) return -1;
    memcpy(b->data + b->size, data, n);
    b->size += n;
    return 0;
}

static int aot_buf_append_str(aot_buffer_t *b, const char *s)
{
    if (!s) s = "";
    return aot_buf_append(b, s, strlen(s) + 1);
}

/* حشو بايتًا بايتًا حتى الوصول إلى الإزاحة المطلوبة تمامًا */
static int aot_buf_pad_to(aot_buffer_t *b, size_t target)
{
    while (b->size < target) {
        uint8_t z = 0;
        if (aot_buf_append(b, &z, 1) < 0) return -1;
    }
    return 0;
}

/* =========================================================
 * جدول النصوص
 * ========================================================= */

typedef struct {
    char   **strings;
    uint32_t *offsets;
    int      count;
    int      capacity;
    aot_buffer_t *string_buf;
} aot_string_table_t;

static void aot_st_init(aot_string_table_t *st, aot_buffer_t *buf)
{
    st->strings = NULL;
    st->offsets = NULL;
    st->count = 0;
    st->capacity = 0;
    st->string_buf = buf;
}

static uint32_t aot_st_add(aot_string_table_t *st, const char *s)
{
    if (!s) s = "";

    for (int i = 0; i < st->count; i++) {
        if (strcmp(st->strings[i], s) == 0)
            return st->offsets[i];
    }

    if (st->count >= st->capacity) {
        int nc = st->capacity ? st->capacity * 2 : 16;
        char **ns = (char **)memory_api.alloc(nc * sizeof(char *));
        uint32_t *no = (uint32_t *)memory_api.alloc(nc * sizeof(uint32_t));
        if (!ns || !no) { if (ns) memory_api.free(ns); if (no) memory_api.free(no); return 0; }

        for (int i = 0; i < st->count; i++) { ns[i] = st->strings[i]; no[i] = st->offsets[i]; }
        if (st->strings) memory_api.free(st->strings);
        if (st->offsets) memory_api.free(st->offsets);
        st->strings = ns;
        st->offsets = no;
        st->capacity = nc;
    }

    uint32_t off = (uint32_t)st->string_buf->size;
    aot_buf_append_str(st->string_buf, s);

    st->strings[st->count] = arp_strdup(s);
    st->offsets[st->count] = off;
    st->count++;

    return off;
}

static void aot_st_free(aot_string_table_t *st)
{
    for (int i = 0; i < st->count; i++)
        if (st->strings[i]) memory_api.free(st->strings[i]);
    if (st->strings) memory_api.free(st->strings);
    if (st->offsets) memory_api.free(st->offsets);
    st->strings = NULL;
    st->offsets = NULL;
    st->count = 0;
    st->capacity = 0;
}

/* =========================================================
 * أسماء المعاملات
 *
 * الكود المولَّد يقرأ المعاملات بأسمائها (متغيرات VM محلية)، لذلك يجب أن
 * يحمل الملف أسماءها؛ بدونها تُسجَّل الدالة بصفر معاملات ويفشل أي استدعاء
 * بمعاملات بصمت (arp_invoke_function_typed يرفض اختلاف العدد).
 * ========================================================= */

/* يكتب أسماء المعاملات كنص واحد "اسم1\nاسم2\n..." في جدول النصوص */
static int aot_add_params(aot_string_table_t *st, const arp_function_t *f,
                          uint32_t *out_off, uint32_t *out_len)
{
    *out_off = 0;
    *out_len = 0;
    if (f->parameter_count <= 0) return 0;
    if (!f->parameters) return -1;

    size_t total = 0;
    for (int i = 0; i < f->parameter_count; i++) {
        if (!f->parameters[i]) return -1;
        total += strlen(f->parameters[i]) + 1;       /* الاسم + فاصل */
    }

    char *tmp = (char *)memory_api.alloc(total + 1);
    if (!tmp) return -1;

    size_t pos = 0;
    for (int i = 0; i < f->parameter_count; i++) {
        size_t n = strlen(f->parameters[i]);
        memcpy(tmp + pos, f->parameters[i], n);
        pos += n;
        if (i + 1 < f->parameter_count) tmp[pos++] = '\n';
    }
    tmp[pos] = '\0';

    *out_off = aot_st_add(st, tmp);
    *out_len = (uint32_t)pos;
    memory_api.free(tmp);
    return 0;
}

/* تسجيل دالة محمّلة من ملف .arpe مع أسماء معاملاتها */
static int aot_register_function(const arp_exec_header_t *hdr,
                                 const uint8_t *strings,
                                 const arp_exec_func_header_t *fh,
                                 const char *name,
                                 void *code)
{
    int argc = (int)fh->param_count;
    if (argc == 0)
        return function_register_aot(name, NULL, 0, code, fh->code_size);

    if (fh->params_len == 0 ||
        (uint64_t)fh->params_offset + fh->params_len >= hdr->strings_size)
        return -1;

    char *tmp = (char *)memory_api.alloc((size_t)fh->params_len + 1);
    if (!tmp) return -1;
    memcpy(tmp, strings + fh->params_offset, fh->params_len);
    tmp[fh->params_len] = '\0';

    const char **names = (const char **)memory_api.alloc((size_t)argc * sizeof(char *));
    if (!names) { memory_api.free(tmp); return -1; }

    int count = 0;
    char *p = tmp;
    while (count < argc) {
        names[count++] = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') { *p = '\0'; p++; }
        else break;
    }

    int rc = -1;
    if (count == argc && *p == '\0')
        rc = function_register_aot(name, names, argc, code, fh->code_size);

    memory_api.free(names);
    memory_api.free(tmp);
    return rc;
}

/* =========================================================
 * الوظيفة الرئيسية: ترجمة سكربت إلى ملف تنفيذي
 *
 * تخطيط الملف:
 *   [ الرأس ][ جدول رؤوس الدوال ][ حشو ][ النصوص ][ حشو ][ الكود ]
 *
 * code_offset في كل رأس دالة نسبةً إلى بداية قسم الكود،
 * وموضع قسم الكود نفسه مخزّن في hdr.code_offset.
 * ========================================================= */
int arp_aot_compile(
    const char *script,
    uint32_t    size,
    void      **out_binary,
    size_t     *out_size
)
{
    //aot_diag("=== arp_aot_compile START ===");

    if (!script || size == 0 || !out_binary || !out_size) {
        //aot_diag("ERROR: invalid args");
        return -1;
    }

    *out_binary = NULL;
    *out_size = 0;

    //aot_diag_int("script_size", (int)size);

    /* 1) إزالة التعليقات */
    char *cleaned = NULL;
    uint32_t cleaned_size = 0;
    if (comment_remove(script, size, &cleaned, &cleaned_size) < 0) {
        //aot_diag("ERROR: comment_remove failed");
        return -1;
    }

    //aot_diag_int("cleaned_size", (int)cleaned_size);

    const char *exec_script = cleaned ? cleaned : script;
    uint32_t exec_size = cleaned ? cleaned_size : size;

    /* 2) جمع الدوال */
    //aot_diag("calling arp_compile_only...");
    if (arp_compile_only(exec_script, exec_size) < 0) {
        //aot_diag("ERROR: arp_compile_only failed");
        if (cleaned) memory_api.free(cleaned);
        return -1;
    }

    //aot_diag("arp_compile_only OK");

    arp_context_t *ctx = arp_get_current_context();
    if (!ctx) {
        //aot_diag("ERROR: no context");
        if (cleaned) memory_api.free(cleaned);
        return -1;
    }

    //aot_diag_int("function_count", ctx->function_count);

    if (ctx->function_count == 0) {
        //aot_diag("WARNING: no functions registered");
    }

    /* 3) بناء الملف */
    aot_buffer_t strings_buf, code_buf, func_headers, reloc_buf;

    if (aot_buf_init(&strings_buf, 4096) < 0) {
        if (cleaned) memory_api.free(cleaned);
        return -1;
    }
    if (aot_buf_init(&code_buf, 65536) < 0) {
        memory_api.free(strings_buf.data);
        if (cleaned) memory_api.free(cleaned);
        return -1;
    }
    if (aot_buf_init(&func_headers, 4096) < 0) {
        memory_api.free(strings_buf.data);
        memory_api.free(code_buf.data);
        if (cleaned) memory_api.free(cleaned);
        return -1;
    }
    if (aot_buf_init(&reloc_buf, 4096) < 0) {
        memory_api.free(strings_buf.data);
        memory_api.free(code_buf.data);
        memory_api.free(func_headers.data);
        if (cleaned) memory_api.free(cleaned);
        return -1;
    }

    uint32_t total_relocs = 0;

    aot_string_table_t st;
    aot_st_init(&st, &strings_buf);

    int func_count = 0;
    uint32_t entry_offset = 0, entry_size = 0;

    for (int i = 0; i < ctx->function_count; i++) {
        arp_function_t *f = &ctx->functions[i];

        if (!f->name) continue;

        //development_screen_print("[AOT] compiling fn: ");
        //development_screen_println(f->name);
        development_screen_update();

        //aot_diag_int("  line_count", f->line_count);

        void *code = NULL;
        size_t code_size = 0;
        jit_reloc_t *relocs = NULL;
        uint32_t reloc_n = 0;

        if (f->lines && f->line_count > 0) {
            /* ★ ترجمة بلا عناوين مطلقة: كل استدعاء مساعد يُسجَّل كترحيل */
            if (machine_jit_compile_function_reloc(
                    (const char **)f->lines, f->line_count,
                    &code, &code_size, &relocs, &reloc_n) < 0) {
                //aot_diag("  ERROR: machine_jit_compile_function_reloc failed");
                if (code) memory_api.free(code);
                if (relocs) memory_api.free(relocs);
                goto fail_buffers;
            }
        } else {
            code_size = sizeof(g_empty_fn_code);
            code = memory_api.alloc(code_size);
            if (!code) goto fail_buffers;
            memcpy(code, g_empty_fn_code, code_size);
        }

        //aot_diag_int("  code_size", (int)code_size);
        //aot_diag_int("  relocs", (int)reloc_n);

        uint32_t name_off = aot_st_add(&st, f->name);

        uint32_t params_off = 0, params_len = 0;
        if (aot_add_params(&st, f, &params_off, &params_len) < 0) {
            memory_api.free(code);
            if (relocs) memory_api.free(relocs);
            goto fail_buffers;
        }

        /* الإزاحة نسبةً إلى بداية قسم الكود */
        uint32_t code_off = (uint32_t)code_buf.size;
        if (aot_buf_append(&code_buf, code, code_size) < 0) {
            memory_api.free(code);
            if (relocs) memory_api.free(relocs);
            goto fail_buffers;
        }
        memory_api.free(code);

        /* ★ ترحيلات هذه الدالة (offset نسبةً إلى بداية كود الدالة) */
        uint32_t reloc_first = total_relocs;
        if (reloc_n > 0) {
            if (aot_buf_append(&reloc_buf, relocs,
                               (size_t)reloc_n * sizeof(jit_reloc_t)) < 0) {
                memory_api.free(relocs);
                goto fail_buffers;
            }
            total_relocs += reloc_n;
        }
        if (relocs) memory_api.free(relocs);

        arp_exec_func_header_t fh;
        memset(&fh, 0, sizeof(fh));
        fh.name_offset = name_off;
        fh.name_len    = (uint32_t)strlen(f->name);
        fh.code_offset = code_off;
        fh.code_size   = (uint32_t)code_size;
        fh.param_count = (uint16_t)f->parameter_count;
        fh.flags       = 0;
        fh.reloc_first = reloc_first;
        fh.reloc_count = reloc_n;
        fh.params_offset = params_off;
        fh.params_len    = params_len;

        if (aot_buf_append(&func_headers, &fh, sizeof(fh)) < 0)
            goto fail_buffers;
        func_count++;

        if (strcmp(f->name, "__main__") == 0) {
            entry_offset = code_off;
            entry_size   = (uint32_t)code_size;
            //aot_diag("  found __main__, setting entry");
        }
    }

    //aot_diag_int("total func_count", func_count);
    //aot_diag_int("entry_size", (int)entry_size);

    /* إذا لم توجد __main__ — لا فشل، لكن سيكون ملف بلا نقطة دخول */
    if (entry_size == 0) {
        //aot_diag("WARNING: no __main__ found (flags=0)");
    }

    /* 4) بناء الرأس */
    arp_exec_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = ARP_EXEC_MAGIC;
    hdr.version = ARP_EXEC_VERSION;
    hdr.entry_offset = entry_offset;
    hdr.entry_size = entry_size;
    hdr.function_count = (uint32_t)func_count;
    hdr.flags = (entry_size > 0) ? 1 : 0;

    uint32_t header_size = sizeof(arp_exec_header_t);
    uint32_t funcs_size = func_count * sizeof(arp_exec_func_header_t);

    hdr.strings_offset = AOT_ALIGN(header_size + funcs_size, 16);
    hdr.strings_size = (uint32_t)strings_buf.size;

    uint32_t code_offset_start =
        AOT_ALIGN(hdr.strings_offset + hdr.strings_size, 16);

    /* ★ موضع وحجم قسم الكود يُخزَّنان في الرأس */
    hdr.code_offset = code_offset_start;
    hdr.code_size   = (uint32_t)code_buf.size;

    /* ★ قسم الترحيلات بعد الكود (محاذاة 8) */
    uint32_t reloc_offset_start =
        AOT_ALIGN(code_offset_start + (uint32_t)code_buf.size, 8);
    hdr.reloc_offset = reloc_offset_start;
    hdr.reloc_count  = total_relocs;

    //aot_diag_int("strings_offset", (int)hdr.strings_offset);
    //aot_diag_int("code_offset", (int)hdr.code_offset);
    //aot_diag_int("code_section_size", (int)hdr.code_size);
    //aot_diag_int("reloc_offset", (int)hdr.reloc_offset);
    //aot_diag_int("reloc_count", (int)hdr.reloc_count);

    /* 5) تجميع كل شيء */
    aot_buffer_t final_buf;
    if (aot_buf_init(&final_buf,
                     (size_t)reloc_offset_start + reloc_buf.size + 64) < 0)
        goto fail_buffers;

    int build_ok =
        aot_buf_append(&final_buf, &hdr, sizeof(hdr)) == 0 &&
        aot_buf_append(&final_buf, func_headers.data, func_headers.size) == 0 &&
        aot_buf_pad_to(&final_buf, hdr.strings_offset) == 0 &&
        aot_buf_append(&final_buf, strings_buf.data, strings_buf.size) == 0 &&
        aot_buf_pad_to(&final_buf, code_offset_start) == 0 &&
        aot_buf_append(&final_buf, code_buf.data, code_buf.size) == 0 &&
        aot_buf_pad_to(&final_buf, reloc_offset_start) == 0 &&
        (reloc_buf.size == 0 ||
         aot_buf_append(&final_buf, reloc_buf.data, reloc_buf.size) == 0);

    if (!build_ok) {
        //aot_diag("ERROR: assembling final buffer failed");
        memory_api.free(final_buf.data);
        goto fail_buffers;
    }

    /* 6) تنظيف */
    aot_st_free(&st);
    memory_api.free(strings_buf.data);
    memory_api.free(code_buf.data);
    memory_api.free(func_headers.data);
    memory_api.free(reloc_buf.data);

    *out_binary = final_buf.data;
    *out_size = final_buf.size;

    //aot_diag_int("final_binary_size", (int)final_buf.size);
    //aot_diag("=== arp_aot_compile SUCCESS ===");

    if (cleaned) memory_api.free(cleaned);
    return 0;

fail_buffers:
    //aot_diag("=== arp_aot_compile FAILED (buffer error) ===");
    aot_st_free(&st);
    memory_api.free(strings_buf.data);
    memory_api.free(code_buf.data);
    memory_api.free(func_headers.data);
    memory_api.free(reloc_buf.data);
    if (cleaned) memory_api.free(cleaned);
    return -1;
}

/* =========================================================
 * ★ تطبيق الترحيلات على كود دالة بعد نسخه إلى الذاكرة النهائية
 *
 * لكل ترحيل: نكتب عنوان الرمز (في هذه النواة) داخل imm64.
 * هذا ما يعوّض أن ملف .arpe لا يحمل عناوين مطلقة أبدًا.
 * ========================================================= */
static int aot_apply_relocs(const arp_exec_header_t *hdr,
                            const void *binary, size_t size,
                            const arp_exec_func_header_t *fh,
                            uint8_t *code)
{
    if (fh->reloc_count == 0) return 0;

    /* حدود قسم الترحيلات */
    uint64_t rel_end = (uint64_t)hdr->reloc_offset +
                       (uint64_t)hdr->reloc_count * sizeof(jit_reloc_t);
    if (rel_end > size) {
        //aot_diag("ERROR: reloc section out of range");
        return -1;
    }
    if ((uint64_t)fh->reloc_first + fh->reloc_count > hdr->reloc_count) {
        //aot_diag("ERROR: function relocs out of range");
        return -1;
    }

    const jit_reloc_t *rel =
        (const jit_reloc_t *)((const uint8_t *)binary + hdr->reloc_offset);

    for (uint32_t k = 0; k < fh->reloc_count; k++) {
        const jit_reloc_t *r = &rel[fh->reloc_first + k];

        if ((uint64_t)r->offset + 8 > fh->code_size) {
            //aot_diag("ERROR: reloc offset out of code");
            return -1;
        }

        void *target = machine_jit_resolve_symbol(r->symbol);
        if (!target) {
            //aot_diag_int("ERROR: unknown symbol id", (int)r->symbol);
            return -1;
        }

        uint64_t addr = (uint64_t)(uintptr_t)target;
        memcpy(code + r->offset, &addr, sizeof(addr));
    }
    return 0;
}

/* =========================================================
 * تنفيذ ملف تنفيذي مُترجَم مسبقًا
 * ========================================================= */
int arp_aot_execute(const void *binary, size_t size)
{
    //aot_diag("=== arp_aot_execute START ===");

    if (!binary || size < sizeof(arp_exec_header_t)) {
        //aot_diag("ERROR: invalid binary/size");
        return -1;
    }

    const arp_exec_header_t *hdr = (const arp_exec_header_t *)binary;

    //aot_diag_int("magic", (int)hdr->magic);
    //aot_diag_int("version", (int)hdr->version);
    //aot_diag_int("function_count", (int)hdr->function_count);
    //aot_diag_int("flags", (int)hdr->flags);
    //aot_diag_int("entry_offset", (int)hdr->entry_offset);
    //aot_diag_int("entry_size", (int)hdr->entry_size);
    //aot_diag_int("strings_offset", (int)hdr->strings_offset);
    //aot_diag_int("code_offset", (int)hdr->code_offset);
    //aot_diag_int("code_size", (int)hdr->code_size);

    if (hdr->magic != ARP_EXEC_MAGIC) {
        //aot_diag("ERROR: bad magic");
        return -1;
    }
    if (hdr->version != ARP_EXEC_VERSION) {
        //aot_diag("ERROR: bad version (rebuild the executable)");
        return -1;
    }

    /* ★ تحقق من حدود الأقسام قبل لمس أي بايت */
    uint64_t funcs_end = (uint64_t)sizeof(arp_exec_header_t) +
        (uint64_t)hdr->function_count * sizeof(arp_exec_func_header_t);
    if (funcs_end > size) {
        //aot_diag("ERROR: function table out of range");
        return -1;
    }
    if ((uint64_t)hdr->strings_offset + hdr->strings_size > size) {
        //aot_diag("ERROR: strings section out of range");
        return -1;
    }
    if ((uint64_t)hdr->code_offset + hdr->code_size > size) {
        //aot_diag("ERROR: code section out of range");
        return -1;
    }
    if ((uint64_t)hdr->reloc_offset +
        (uint64_t)hdr->reloc_count * sizeof(jit_reloc_t) > size) {
        //aot_diag("ERROR: reloc section out of range");
        return -1;
    }

    /* احفظ السياق الحالي */
    arp_context_t *saved_ctx = arp_get_current_context();
    //aot_diag_int("saved_ctx_is_null", saved_ctx ? 0 : 1);

    static arp_context_t local_ctx_storage;
    arp_context_t *ctx = saved_ctx;

    if (!ctx) {
        memset(&local_ctx_storage, 0, sizeof(local_ctx_storage));
        ctx = &local_ctx_storage;
        arp_set_current_context(ctx);
        //aot_diag("using local_ctx_storage");
    }

    //aot_diag("calling functions_init...");
    functions_init();
    //aot_diag("calling variables_init...");
    variables_init();
    //aot_diag("calling structs_init...");
    structs_init();

    const uint8_t *base      = (const uint8_t *)binary;
    const uint8_t *strings   = base + hdr->strings_offset;
    const uint8_t *code_base = base + hdr->code_offset;   /* ★ قسم الكود */
    const arp_exec_func_header_t *funcs =
        (const arp_exec_func_header_t *)(base + sizeof(arp_exec_header_t));

    //aot_diag("registering functions...");

    for (uint32_t i = 0; i < hdr->function_count; i++) {
        const arp_exec_func_header_t *fh = &funcs[i];

        /* ★ تحقق من حدود الاسم والكود لهذه الدالة */
        if ((uint64_t)fh->name_offset + fh->name_len >= hdr->strings_size) {
            //aot_diag("ERROR: bad function name bounds");
            if (!saved_ctx) arp_set_current_context(NULL);
            return -1;
        }
        if (fh->code_size == 0 ||
            (uint64_t)fh->code_offset + fh->code_size > hdr->code_size) {
            //aot_diag("ERROR: bad function code bounds");
            if (!saved_ctx) arp_set_current_context(NULL);
            return -1;
        }

        const char *name = (const char *)(strings + fh->name_offset);

        //development_screen_print("[AOT]   register: ");
        //development_screen_println(name);
        development_screen_update();

        //aot_diag_int("    code_size", (int)fh->code_size);

        void *code = memory_api.alloc(fh->code_size);
        if (!code) {
            //aot_diag("ERROR: alloc code failed");
            if (!saved_ctx) arp_set_current_context(NULL);
            return -1;
        }

        /* ★ النسخ من قسم الكود وليس من بداية الملف */
        memcpy(code, code_base + fh->code_offset, fh->code_size);

        /* ★ الترحيل الديناميكي: اكتب عناوين الدوال المساعدة الحقيقية */
        if (aot_apply_relocs(hdr, binary, size, fh, (uint8_t *)code) < 0) {
            //aot_diag("ERROR: relocation failed");
            memory_api.free(code);
            if (!saved_ctx) arp_set_current_context(NULL);
            return -1;
        }

        if (aot_register_function(hdr, strings, fh, name, code) < 0) {
            //aot_diag("ERROR: function_register_aot failed");
            memory_api.free(code);
            if (!saved_ctx) arp_set_current_context(NULL);
            return -1;
        }
    }

    //aot_diag("all functions registered");

    /* نفّذ __main__ إن وُجد */
    if (hdr->flags & 1) {
        //aot_diag("flags has bit 0, executing __main__...");

        arp_function_t *main_fn = function_find("__main__");
        if (!main_fn) {
            //aot_diag("ERROR: __main__ not found");
            if (!saved_ctx) arp_set_current_context(NULL);
            return -1;
        }
        if (!main_fn->jit_code) {
            //aot_diag("ERROR: __main__ has no jit_code");
            if (!saved_ctx) arp_set_current_context(NULL);
            return -1;
        }

        /* ★ ABI النوعي الكامل */
        jit_result_t jr;
        jr.type    = ARP_TYPE_UNKNOWN;
        jr.numeric = 0.0;
        jr.text    = NULL;

        if (machine_jit_execute_typed(main_fn->jit_code, &jr) != 0) {
            //aot_diag("ERROR: machine_jit_execute_typed failed");
            jit_result_free(&jr);
            if (!saved_ctx) arp_set_current_context(NULL);
            return -1;
        }
        jit_result_free(&jr);

        //aot_diag("__main__ executed OK");
    } else {
        //aot_diag("flags has no bit 0, skipping __main__");
    }

    if (!saved_ctx) {
        arp_set_current_context(NULL);
    }

    //aot_diag("=== arp_aot_execute SUCCESS ===");
    return 0;
}

/* =========================================================
 * arp_aot_execute_library — تسجيل الدوال في السياق الحالي
 *                            (بدون مسحه) ثم تنفيذ __main__.
 * ========================================================= */
int arp_aot_execute_library(const void *binary, size_t size)
{
    //aot_diag("=== arp_aot_execute_library START ===");

    if (!binary || size < sizeof(arp_exec_header_t)) {
        //aot_diag("ERROR: invalid binary/size");
        return -1;
    }

    const arp_exec_header_t *hdr = (const arp_exec_header_t *)binary;

    if (hdr->magic != ARP_EXEC_MAGIC) return -1;
    if (hdr->version != ARP_EXEC_VERSION) return -1;

    /* تحقق من الحدود */
    uint64_t funcs_end = (uint64_t)sizeof(arp_exec_header_t) +
        (uint64_t)hdr->function_count * sizeof(arp_exec_func_header_t);
    if (funcs_end > size) return -1;
    if ((uint64_t)hdr->strings_offset + hdr->strings_size > size) return -1;
    if ((uint64_t)hdr->code_offset + hdr->code_size > size) return -1;
    if ((uint64_t)hdr->reloc_offset +
        (uint64_t)hdr->reloc_count * sizeof(jit_reloc_t) > size) return -1;

    /* ★ لا نستدعي functions_init/variables_init/structs_init هنا.
     * المكتبة تُضاف إلى السياق الحالي. */

    const uint8_t *base      = (const uint8_t *)binary;
    const uint8_t *strings   = base + hdr->strings_offset;
    const uint8_t *code_base = base + hdr->code_offset;
    const arp_exec_func_header_t *funcs =
        (const arp_exec_func_header_t *)(base + sizeof(arp_exec_header_t));

    for (uint32_t i = 0; i < hdr->function_count; i++) {
        const arp_exec_func_header_t *fh = &funcs[i];

        if ((uint64_t)fh->name_offset + fh->name_len >= hdr->strings_size)
            return -1;
        if (fh->code_size == 0 ||
            (uint64_t)fh->code_offset + fh->code_size > hdr->code_size)
            return -1;

        const char *name = (const char *)(strings + fh->name_offset);

        void *code = memory_api.alloc(fh->code_size);
        if (!code) return -1;

        memcpy(code, code_base + fh->code_offset, fh->code_size);

        /* ★ الترحيل الديناميكي */
        if (aot_apply_relocs(hdr, binary, size, fh, (uint8_t *)code) < 0) {
            memory_api.free(code);
            return -1;
        }

        if (aot_register_function(hdr, strings, fh, name, code) < 0) {
            memory_api.free(code);
            return -1;
        }
    }

    //aot_diag("library functions registered");

    if (hdr->flags & 1) {
        arp_function_t *main_fn = function_find("__main__");
        if (!main_fn || !main_fn->jit_code) {
            //aot_diag("ERROR: __main__ not found or not compiled");
            return -1;
        }

        /* ★ ABI النوعي الكامل */
        jit_result_t jr;
        jr.type    = ARP_TYPE_UNKNOWN;
        jr.numeric = 0.0;
        jr.text    = NULL;

        if (machine_jit_execute_typed(main_fn->jit_code, &jr) != 0) {
            //aot_diag("ERROR: executing library __main__ failed");
            jit_result_free(&jr);
            return -1;
        }
        jit_result_free(&jr);
    }

    //aot_diag("=== arp_aot_execute_library SUCCESS ===");
    return 0;
}
