#include "محرك_تواصل_العمليات.h"

#include "محرك_العمليات/محرك_العمليات.h"
#include "محرك_الجدولة/محرك_الجدولة.h"

#include "القلب/الوحدات/وحدات_العمليات/محرك_التطبيقات/الصيغه/تطبيق.h"
#include "القلب/الوحدات/وحدات_العمليات/محرك_التطبيقات/محرك_التطبيقات.h"

#include "القلب/الوحدات/وحدات_العمليات/محرك_التطبيقات/قيم_الملف.h"

#include "القلب/مكتبة_المحركات/مكتبة_معالجة_النصوص.h"

/* ========================================================= */
/* external */
/* ========================================================= */

extern file_t* find_file(
    const char* name
);



/* ========================================================= */
/* INTERRUPTS */
/* ========================================================= */

static inline void disable_interrupts(void)
{
    __asm__ volatile("cli");
}

static inline void enable_interrupts(void)
{
    __asm__ volatile("sti");
}


static ipc_service_t
services[IPC_MAX_SERVICES];

static int service_count = 0;

/* ========================================================= */
/* SERVICES */
/* ========================================================= */

int ipc_register_service(
    const char* name
) {

    process_t* current =
        get_current_process();

    if (!current)
        return -1;

    if (
        service_count >=
        IPC_MAX_SERVICES
    ) {
        return -2;
    }

    strcpy(
        services[service_count].name,
        name
    );

    services[service_count].pid =
        current->pid;

    service_count++;

    return 0;
}

int ipc_find_service(
    const char* name
) {

    for (
        int i = 0;
        i < service_count;
        i++
    ) {

        if (
            strcmp(
                services[i].name,
                name
            ) == 0
        ) {

            return
                services[i].pid;
        }
    }

    return -1;
}

int ipc_send_service(
    const char* name,
    Message* msg
) {

    int pid =
        ipc_find_service(
            name
        );

    if (pid < 0)
        return -1;

    return ipc_send(
        pid,
        msg
    );
}

int ipc_launch_app(
    const char* app_name
) {
    Message msg;

    memset(
        &msg,
        0,
        sizeof(Message)
    );

    msg.type = IPC_LAUNCH_APP;

    strcpy(
        (char*)msg.data,
        app_name
    );

    msg.size =
        strlen(app_name) + 1;

    return ipc_send(
        0,
        &msg
    );
}

/* ========================================================= */
/* INIT */
/* ========================================================= */

void ipc_init_process(
    process_t* p
) {

    p->mailbox.head  = 0;
    p->mailbox.tail  = 0;
    p->mailbox.count = 0;
}

/* ========================================================= */
/* SEND */
/* ========================================================= */

int ipc_send(
    int target_pid,
    Message* msg
) {


    /* ===================================================== */
    /* kernel launch request */
    /* ===================================================== */

    if (
        target_pid == 0 &&
        msg->type == IPC_LAUNCH_APP
    ) {



        file_t* f =
            find_file(
                (char*)msg->data
            );

        if (f) {

            app_run(f);

        } else {

        }

        return 0;
    }

    /* ===================================================== */
    /* normal ipc */
    /* ===================================================== */

    process_t* target =
        get_process(target_pid);

    if (!target) {


        return -1;
    }

    ipc_mailbox_t* box =
        &target->mailbox;

    disable_interrupts();

    if (
        box->count >=
        IPC_MAX_MESSAGES
    ) {

        enable_interrupts();


        return -2;
    }

    box->messages[box->tail] =
        *msg;

    box->tail =
        (box->tail + 1) %
        IPC_MAX_MESSAGES;

    box->count++;

    enable_interrupts();


    if (
        target->state ==
        PROCESS_BLOCKED_IPC
    ) {

        target->state =
            TASK_READY;

        scheduler_add(target);

    }

    return 0;
}

/* ========================================================= */
/* RECEIVE */
/* ========================================================= */

int ipc_receive(
    Message* out_msg
) {

    process_t* current =
        get_current_process();

    if (!current)
        return -1;

    ipc_mailbox_t* box =
        &current->mailbox;

    disable_interrupts();

    while (
        box->count == 0
    ) {

        current->state =
            PROCESS_BLOCKED_IPC;

        enable_interrupts();

        scheduler_remove(current);

        yield();

        disable_interrupts();
    }

    *out_msg =
        box->messages[box->head];

    box->head =
        (box->head + 1) %
        IPC_MAX_MESSAGES;

    box->count--;

    enable_interrupts();


    return 0;
}
