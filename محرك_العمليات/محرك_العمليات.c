#include <string.h>

#include "محرك_العمليات.h"

#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "محرك_الجدولة/بوابة_الجدولة.h"
#include "محرك_تواصل_العمليات/محرك_تواصل_العمليات.h"
#include "محرك_تواصل_العمليات/بوابة_النواه.h"

#include "مدير_عمليات_النواه/مدير_عمليات_النواه.h"

#define KERNEL_CS  0x08
#define KERNEL_SS  0x10
#define RFLAGS_IF  0x202


/* ========================================================= */
/* process_create */
/* ========================================================= */

process_t* process_create(
    void (*entry)(kernel_api_t*)
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

    p->pml4 =
        memory_api.create_address_space();

    if (!p->pml4) {

        memory_api.free(p);

        return 0;
    }

    /* ===================================================== */
    /* PROCESS INFO */
    /* ===================================================== */

    p->entry = (void*)entry;

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
        (uint64_t)stack_top;

    /* ===================================================== */
    /* iretq frame */
    /* ===================================================== */

    *(--stack_top) = KERNEL_SS;
    *(--stack_top) = rsp_val;
    *(--stack_top) = RFLAGS_IF;
    *(--stack_top) = KERNEL_CS;
    *(--stack_top) = (uint64_t)entry;

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
     * rdi = first argument
     * kernel_api_t*
     */

    *(--stack_top) =
        (uint64_t)&kapi; /* rdi */

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

    return next->rsp;
}
