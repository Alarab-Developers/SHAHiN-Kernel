#include "محرك_العمليات.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "محرك_الجدولة/بوابة_الجدولة.h"
#include "محرك_تواصل_العمليات/محرك_تواصل_العمليات.h"

#include "مدير_عمليات_النواه/مدير_عمليات_النواه.h"

#define KERNEL_CS  0x08
#define KERNEL_SS  0x10
#define RFLAGS_IF  0x202


/* ========================================================= */
/* process_create */
/* ========================================================= */

/* ========================================================= */
/* create_process_common */
/* ========================================================= */

/*
 * الجسم المشترك لإنشاء عملية جديدة: PID، مساحة عناوين، مكدس،
 * وبناء إطار iretq/استعادة السجلات فوق المكدس.
 *
 * rdi_value هو ما سيجده entry في أول معامل له عند بدء التشغيل:
 * - process_create تُمرر دائماً &kapi (كما كان الحال سابقاً).
 * - process_create_ex تُمرر "arg" الخاص بالمستدعي.
 *
 * استُخرجت هذه الدالة لتفادي تكرار كود بناء العملية والمكدس مرتين.
 */
static process_t* create_process_common(
    void* entry_ptr,
    uint64_t rdi_value
) {

    int pid =
        kernel_process_allocate_pid();

    if (pid < 0)
        return 0;

    process_t* p =
        (process_t*)
        memory_api.alloc(
            sizeof(process_t)
        );

    if (!p)
        return 0;

    memset(
        p,
        0,
        sizeof(process_t)
    );

    /* ===================================================== */
    /* PID */
    /* ===================================================== */

    p->pid = pid;

    /* ===================================================== */
    /* ADDRESS SPACE */
    /* ===================================================== */

    p->address_space =
        memory_api.create_address_space();
    
    if (!p->address_space) {
    
        memory_api.free(p);

        return 0;
    }

    /* ===================================================== */
    /* PROCESS INFO */
    /* ===================================================== */

    p->entry = entry_ptr;

    p->state = TASK_READY;

    p->vruntime = 0;

    p->nice = 0;

    p->sleep_ticks = 0;

    ipc_init_process(p);

    /* ===================================================== */
    /* STACK */
    /* ===================================================== */

    p->stack =
        memory_api.alloc(65536);

    if (!p->stack) {

        memory_api.free(p);

        return 0;
    }

    memset(
        p->stack,
        0,
        65536
    );

    uint64_t* stack_top =
        (uint64_t*)
        ((uint8_t*)p->stack + 65536);

    uint64_t rsp_val =
         (uint64_t)stack_top - 8;

    /* ===================================================== */
    /* iretq frame */
    /* ===================================================== */

    *(--stack_top) = KERNEL_SS;
    *(--stack_top) = rsp_val;
    *(--stack_top) = RFLAGS_IF;
    *(--stack_top) = KERNEL_CS;
    *(--stack_top) = (uint64_t)entry_ptr;

    /* ===================================================== */
    /* RESTORE_REGS FRAME */
    /* ===================================================== */

    *(--stack_top) = 0; /* rax */
    *(--stack_top) = 0; /* rbx */
    *(--stack_top) = 0; /* rcx */
    *(--stack_top) = 0; /* rdx */
    *(--stack_top) = 0; /* rsi */

    /*
     * IMPORTANT:
     * rdi = أول معامل يستلمه entry عند بدء التشغيل
     */

    *(--stack_top) = rdi_value; /* rdi */

    *(--stack_top) = 0; /* rbp */
    *(--stack_top) = 0; /* r8  */
    *(--stack_top) = 0; /* r9  */
    *(--stack_top) = 0; /* r10 */
    *(--stack_top) = 0; /* r11 */
    *(--stack_top) = 0; /* r12 */
    *(--stack_top) = 0; /* r13 */
    *(--stack_top) = 0; /* r14 */
    *(--stack_top) = 0; /* r15 */

    p->rsp =
        (uint64_t)stack_top;
    kernel_process_register(
        pid,
        p
    );

    return p;
}

/* ========================================================= */
/* process_create */
/* ========================================================= */

process_t* process_create(
    void (*entry)(void*)
) {
    return create_process_common(
        (void*)entry,
        0
    );
}

/* ========================================================= */
/* process_create_ex */
/* ========================================================= */

/*
 * مثل process_create، لكنها تُمرر "arg" الخاص بالمستدعي في rdi
 * بدلاً من &kapi. هذه هي الآلية التي تسمح لكل تطبيق (سكربت ARP)
 * أن يعمل داخل عمليته الخاصة، مع استلام بيانات ملفه (file_t*)
 * مباشرة كمعامل لدالة البداية الخاصة به.
 */
process_t* process_create_ex(
    void (*entry)(void*),
    void* arg
) {
    process_t* p =
        create_process_common(
            (void*)entry,
            (uint64_t)arg
        );

    if (p)
        p->arg = arg;

    return p;
}

/* ========================================================= */
/* INIT */
/* ========================================================= */

void process_init()
{
    kernel_process_manager_init();
}

/* ========================================================= */
/* START */
/* ========================================================= */

void process_start(
    process_t* p
) {

    p->state =
        TASK_READY;

    scheduler_api.add(p);
}

/* ========================================================= */
/* SLEEP */
/* ========================================================= */

void process_sleep(
    process_t* p
) {

    p->state =
        TASK_SLEEPING;

    scheduler_api.sleep(
        p,
        p->sleep_ticks
    );
}

/* ========================================================= */
/* CURRENT */
/* ========================================================= */

process_t* get_current_process()
{
    return scheduler_api.current();
}

/* ========================================================= */
/* GET PROCESS */
/* ========================================================= */

process_t* get_process(
    int pid
)
{
    return kernel_process_get(pid);
}

/* ========================================================= */
/* SWITCH */
/* ========================================================= */

void switch_process(
    process_t* next
)
{
    kernel_process_switch(next);
}




uint64_t process_on_timer_interrupt(
    uint64_t current_rsp
)
{
    process_t* current =
        kernel_process_current();

    if (
        current &&
        current != idle_p
    ) {
        current->rsp =
            current_rsp;
    }

    process_t* next =
    scheduler_api.next();

    if (!next)
        return current_rsp;

    if (next == current)
        return current_rsp;

    kernel_process_switch(next);
    memory_api.switch_address_space(next->address_space);

    return next->rsp;
}
