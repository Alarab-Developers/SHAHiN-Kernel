#ifndef PROCESS_H
#define PROCESS_H

#include "محرك_تواصل_العمليات/محرك_تواصل_العمليات.h"
#include "محرك_الذاكرة/الأدارة/مدير_فضاء_العناوين.h"

#define MAX_PROCESSES 256

extern process_t* idle_p;

typedef struct process {
    int      pid;          /* offset 0  */
    void   (*entry)();     /* offset 8  */
    uint64_t rsp;          /* offset 16 ← isr_timer يستخدم هذا */
    void*    stack;        /* offset 24 */
    int      sleep_ticks;  /* offset 32 */
    int      state;        /* offset 36 */
    uint64_t vruntime;     /* offset 40 */
    int      nice;         /* offset 48 */
    ipc_mailbox_t mailbox; /* offset 52 (حجمه 396 بعد التصغير) */
    address_space_t *address_space;        /* offset 448 */
    void* arg;
    void* app_context;
} process_t;

enum {
    TASK_READY,
    TASK_WAITING,
    TASK_SLEEPING,
    PROCESS_BLOCKED_IPC
};

void process_init();
process_t* process_create(void (*entry)());


process_t* process_create_ex(void (*entry)(void*), void* arg);

void process_start(process_t* p);
void process_sleep(process_t* p);
process_t* get_process(int pid);
process_t* get_current_process();

void process_set_app_context(process_t* p, void* context);
void* process_get_app_context(process_t* p);
void process_exit_current(void);

#endif
