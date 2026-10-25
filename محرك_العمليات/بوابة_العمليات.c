#include "بوابة_العمليات.h"

//  اربط مع المحركات الحقيقية
#include "محرك_العمليات/محرك_العمليات.h"

/*


بوابة العمليات ،هي الواجهة الرئيسية لاستدعائات محرك العمليات الذي بدورة المسؤل عن انشاء وادارة العمليات الخاصة بالنواة
والتطبيقات (بشكل معزول)


*/



/* ================= Wrappers ================= */

static process_t* api_create(void (*entry)()) {
    return process_create(entry);
}

static void api_start(process_t* p) {
    process_start(p);
}

static process_t* api_current() {
    return get_current_process();
}

static uint64_t api_get_pid(process_t* p) {
    return p->pid;
}

static void api_init() {
    process_init();
}

static process_t* api_create_ex(void (*entry)(void*), void* arg) {
    return process_create_ex(entry, arg);
}

static void api_set_app_context(
    process_t* p,
    void* context
)
{
    process_set_app_context(
        p,
        context
    );
}

static void* api_get_app_context(
    process_t* p
)
{
    return process_get_app_context(
        p
    );
}

static void api_exit(void) {
    process_exit_current();
}



/*

 البوابة(الواجهة)

*/

/* ================= API ================= */

process_api_t process_api = {
    .create     = api_create,
    .start      = api_start,
    .current    = api_current,
    .get_pid    = api_get_pid,
    .init       = api_init,
    .create_ex  = api_create_ex,
    .set_app_context = api_set_app_context,
    .get_app_context = api_get_app_context,
    .exit       = api_exit
};
