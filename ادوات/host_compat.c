/* =========================================================
 * tools/host_compat.c
 * طبقة توافق لتشغيل كود اللغة على المضيف (Linux)
 * ========================================================= */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "القلب/الوحدات/محمل_التطبيقات/سياق_اللغة.h"
#include "القلب/الوحدات/محمل_التطبيقات/قيم_الملف.h"

/* =========================================================
 * 1) memory_api — كل الحقول (المتبقي = NULL مقبول)
 * ========================================================= */
static void     *host_alloc(size_t n)         { return malloc(n); }
static void      host_free(void *p)           { free(p); }
static uint64_t  host_alloc_page(void)        { return (uint64_t)(uintptr_t)malloc(4096); }
static void      host_free_page(uint64_t a)   { free((void *)(uintptr_t)a); }
static void      host_map(uint64_t v, uint64_t p, uint64_t f) { (void)v;(void)p;(void)f; }
static void      host_map_to(uint64_t *pm, uint64_t v, uint64_t p, uint64_t f)
                                              { (void)pm;(void)v;(void)p;(void)f; }
static void      host_api_init(void)          { }
static uint64_t *host_get_pml4(void)          { return NULL; }
static address_space_t *host_create_as(void)  { return NULL; }
static void      host_destroy_as(address_space_t *s) { (void)s; }
static void      host_switch_as(address_space_t *s)  { (void)s; }
static address_space_t *host_get_as(void)     { return NULL; }

memory_api_t memory_api = {
    .alloc                     = host_alloc,
    .free                      = host_free,
    .alloc_page                = host_alloc_page,
    .free_page                 = host_free_page,
    .map                       = host_map,
    .map_to                    = host_map_to,
    .init                      = host_api_init,
    .get_pml4                  = host_get_pml4,
    .create_address_space      = host_create_as,
    .destroy_address_space     = host_destroy_as,
    .switch_address_space      = host_switch_as,
    .get_current_address_space = host_get_as,
};

/* =========================================================
 * 2) memory_raw_* — بدائل وهمية كافية للترجمة
 * ========================================================= */
int      memory_raw_write32(uint64_t a, uint32_t v) { (void)a;(void)v; return 0; }
int      memory_raw_read32 (uint64_t a, uint32_t *v){ (void)a;(void)v; return -1; }
int      memory_raw_write8 (uint64_t a, uint8_t v)  { (void)a;(void)v; return 0; }
int      memory_raw_read8  (uint64_t a, uint8_t *v) { (void)a;(void)v; return -1; }
int      memory_raw_write16(uint64_t a, uint16_t v) { (void)a;(void)v; return 0; }
int      memory_raw_read16 (uint64_t a, uint16_t *v){ (void)a;(void)v; return -1; }
int      memory_raw_write64(uint64_t a, uint64_t v) { (void)a;(void)v; return 0; }
int      memory_raw_read64 (uint64_t a, uint64_t *v){ (void)a;(void)v; return -1; }
int      memory_raw_copy    (uint64_t d, uint64_t s, uint64_t n)
                                                    { (void)d;(void)s;(void)n; return -1; }
int      memory_raw_set     (uint64_t a, uint8_t v, uint64_t n)
                                                    { (void)a;(void)v;(void)n; return 0; }
int      memory_raw_compare (uint64_t a, uint64_t b, uint64_t n, int *r)
                                                    { (void)a;(void)b;(void)n;(void)r; return -1; }
uint64_t memory_raw_alloc   (uint64_t n)            { return (uint64_t)(uintptr_t)malloc(n); }
int      memory_raw_free    (uint64_t a)            { free((void *)(uintptr_t)a); return 0; }
uint64_t memory_raw_alloc_page(void)                { return (uint64_t)(uintptr_t)malloc(4096); }
int      memory_raw_map     (uint64_t v, uint64_t p, uint64_t f)
                                                    { (void)v;(void)p;(void)f; return -1; }

/* =========================================================
 * 3) سياق اللغة على المضيف
 * ========================================================= */
static arp_context_t g_host_ctx;

arp_context_t *arp_get_current_context(void)   { return &g_host_ctx; }
void           arp_set_current_context(arp_context_t *c) { (void)c; }
void           arp_set_override_context(arp_context_t *c){ (void)c; }

/* =========================================================
 * 4) find_file — قراءة حقيقية من القرص (لدعم استدعاء)
 *    - الذاكرة التي نعود بها تبقى صالحة حتى الاستدعاء التالي.
 * ========================================================= */
file_t *find_file(const char *name)
{
    static file_t   s_file;
    static uint8_t *s_buf = NULL;

    if (s_buf) { free(s_buf); s_buf = NULL; }
    memset(&s_file, 0, sizeof(s_file));

    if (!name || !name[0]) return NULL;

    FILE *fp = fopen(name, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (sz <= 0) { fclose(fp); return NULL; }

    s_buf = (uint8_t *)malloc((size_t)sz);
    if (!s_buf) { fclose(fp); return NULL; }

    if (fread(s_buf, 1, (size_t)sz, fp) != (size_t)sz) {
        free(s_buf); s_buf = NULL; fclose(fp); return NULL;
    }
    fclose(fp);

    s_file.data = s_buf;
    s_file.size = (uint32_t)sz;
    return &s_file;
}

/* =========================================================
 * 5) شاشة التطوير
 * ========================================================= */
void development_screen_print   (const char *s)  { (void)s; }
void development_screen_println (const char *s)  { (void)s; }
void development_screen_update  (void)           { }
void development_screen_print_hex64(uint64_t v)  { (void)v; }
void development_screen_init    (void)           { }

/* =========================================================
 * 6) execute_* — كل ما يستدعيه مترجم_اللغة.c
 *    (توقيعات مطابقة لبوابة_التنفيذ.h)
 * ========================================================= */
int execute_app(const char *n)                          { (void)n; return -1; }

int execute_create_window_bg(const char *t, uint32_t c, const char *p)
                                                        { (void)t;(void)c;(void)p; return -1; }
int execute_create_window_color(const char *t, uint32_t c)
                                                        { (void)t;(void)c; return -1; }
int execute_create_button(const char *t, int x, int y, int w, int h,
                          const char *fn, const char *bg)
                                                        { (void)t;(void)x;(void)y;(void)w;(void)h;(void)fn;(void)bg; return -1; }
int execute_page_start(void)                            { return 0; }
int execute_page_end(void)                              { return 0; }
int execute_page_background(const char *p)              { (void)p; return 0; }
int execute_system_background(const char *p)            { (void)p; return 0; }

int     execute_hardware(uint16_t port, uint8_t v)      { (void)port;(void)v; return 0; }
uint8_t execute_read_hardware(uint16_t port)            { (void)port; return 0; }

int execute_memory_write  (uint64_t a, uint32_t v)      { (void)a;(void)v; return 0; }
int execute_memory_read   (uint64_t a, uint32_t *v)     { (void)a;(void)v; return -1; }
int execute_memory_set32  (uint64_t a, uint32_t v, uint64_t c)
                                                        { (void)a;(void)v;(void)c; return 0; }
int execute_memory_set32_blend(uint64_t a, uint32_t v, uint64_t c)
                                                        { (void)a;(void)v;(void)c; return 0; }
uint8_t execute_byte_at(uint64_t address)
{
    if (address == 0) return 0;
    return *((uint8_t *)(uintptr_t)address);
}
int execute_memory_write8 (uint64_t a, uint8_t v)       { (void)a;(void)v; return 0; }
int execute_memory_read8  (uint64_t a, uint8_t *v)      { (void)a;(void)v; return -1; }
int execute_memory_write16(uint64_t a, uint16_t v)      { (void)a;(void)v; return 0; }
int execute_memory_read16 (uint64_t a, uint16_t *v)     { (void)a;(void)v; return -1; }
int execute_memory_write64(uint64_t a, uint64_t v)      { (void)a;(void)v; return 0; }
int execute_memory_read64 (uint64_t a, uint64_t *v)     { (void)a;(void)v; return -1; }
int execute_memory_copy   (uint64_t d, uint64_t s, uint64_t n)
                                                        { (void)d;(void)s;(void)n; return -1; }
int execute_memory_set    (uint64_t a, uint8_t v, uint64_t n)
                                                        { (void)a;(void)v;(void)n; return 0; }
int execute_memory_compare(uint64_t a, uint64_t b, uint64_t n, int *r)
                                                        { (void)a;(void)b;(void)n;(void)r; return -1; }
int execute_memory_alloc  (uint64_t n, uint64_t *a)     { (void)n;(void)a; return -1; }
int execute_memory_free   (uint64_t a)                  { (void)a; return 0; }
int execute_memory_alloc_page(uint64_t *a)              { (void)a; return -1; }
int execute_memory_map    (uint64_t v, uint64_t p, uint64_t f)
                                                        { (void)v;(void)p;(void)f; return -1; }

int execute_wait(uint64_t n)                            { (void)n; return 0; }
int execute_keyboard_value(unsigned char k)             { (void)k; return 0; }
int execute_input(const char *v)                        { (void)v; return -1; }

int      execute_text_len(const char *t)                { return t ? (int)strlen(t) : 0; }
int      execute_text_char(const char *t, int i)        { (void)t;(void)i; return -1; }
uint64_t execute_file_pointer(const char *n)            { (void)n; return 0; }
int      execute_rtc_get(int f, int *v)                 { (void)f;(void)v; return -1; }

int execute_print_line         (const char *t)          { (void)t; return 0; }
int execute_print_line_color   (const char *t, uint32_t c) { (void)t;(void)c; return 0; }
int execute_print_inline_color (const char *t, uint32_t c) { (void)t;(void)c; return 0; }
int execute_print_inline       (const char *t)          { (void)t; return 0; }

void execute_begin_horizontal_table(int x, int y, int w, int h)
                                                        { (void)x;(void)y;(void)w;(void)h; }
void execute_end_horizontal_table(void)                 { }
void execute_reset_layout(void)                         { }

int execute_set_cursor_image(const char *p)             { (void)p; return 0; }
int execute_set_min_button_image(const char *p)         { (void)p; return 0; }
int execute_set_max_button_image(const char *p)         { (void)p; return 0; }
int execute_set_close_button_image(const char *p)       { (void)p; return 0; }
int execute_set_system_font(const char *p)              { (void)p; return 0; }
int execute_set_system_font_color(const char *p, uint32_t c)
                                                        { (void)p;(void)c; return 0; }

int execute_create_image_box(int x, int y, int w, int h, const char *p)
                                                        { (void)x;(void)y;(void)w;(void)h;(void)p; return -1; }
int execute_create_text_box(const char *t, int x, int y, int w, int h, uint32_t c)
                                                        { (void)t;(void)x;(void)y;(void)w;(void)h;(void)c; return -1; }
int execute_create_free_window(int x, int y, int w, int h,
                               int r, uint32_t c, int d)
                                                        { (void)x;(void)y;(void)w;(void)h;(void)r;(void)c;(void)d; return -1; }

void execute_event_loop(void)                           { }
int  execute_grid_begin(int c,int r,int mx,int my,int gx,int gy)
                                                        { (void)c;(void)r;(void)mx;(void)my;(void)gx;(void)gy; return -1; }
int  execute_grid_end(void)                             { return 0; }
int  execute_present_screen(void)                       { return 0; }
int  execute_plot_pixel(int x, int y, uint32_t c)       { (void)x;(void)y;(void)c; return -1; }
int  execute_fill_rect (int x, int y, int w, int h, uint32_t c)
                                                        { (void)x;(void)y;(void)w;(void)h;(void)c; return -1; }
uint64_t execute_get_back_buffer_ptr (void)             { return 0; }
uint64_t execute_get_front_buffer_ptr(void)             { return 0; }

/* =========================================================
 * 7) خادم العرض — ما يحتاجه مترجم_اللغة.c فقط
 * ========================================================= */
int ds_create_free_window(int x, int y, int w, int h, int r, uint32_t c, int d)
                                                        { (void)x;(void)y;(void)w;(void)h;(void)r;(void)c;(void)d; return -1; }

int ds_surface_create(int win_id, int x, int y, int w, int h)
                                                        { (void)win_id;(void)x;(void)y;(void)w;(void)h; return -1; }
uint32_t *ds_surface_get_pixels(int win_id, int sid, int *w, int *h)
                                                        { (void)win_id;(void)sid; if (w) *w=0; if (h) *h=0; return NULL; }
void ds_surface_destroy(int win_id, int sid)            { (void)win_id;(void)sid; }
void ds_surface_move(int win_id, int sid, int nx, int ny){ (void)win_id;(void)sid;(void)nx;(void)ny; }
void ds_surface_set_visible(int win_id, int sid, int v) { (void)win_id;(void)sid;(void)v; }

/* =========================================================
 * 8) أدوات إضافية
 * ========================================================= */

/* لغة.c */
void ds_drop_pending_arp_calls(void *ctx) { (void)ctx; }
void ds_close_windows_owned_by(void *ctx) { (void)ctx; }

/* المتغيرات.c — فيديو */
uint32_t  fb_get_width (void)           { return 1024; }
uint32_t  fb_get_height(void)           { return 768; }
uint32_t  fb_get_pitch (void)           { return 1024 * 4; }
uint32_t *fb_get_back_buffer (void)     { return NULL; }
uint32_t *fb_get_front_buffer(void)     { return NULL; }

/* المتغيرات.c — فأرة */
int mouse_x  = 0;
int mouse_y  = 0;
int mouse_dx = 0;
int mouse_dy = 0;
int mouse_left = 0;
int mouse_consume_click(void) { return 0; }

/* الشروط.c — لوحة مفاتيح */
int keyboard_condition(const char *line) { (void)line; return 0; }
