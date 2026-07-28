#include <string.h>

#include "مدير_عمليات_النواه.h"

process_t* current_process = 0;

static process_t* process_table[MAX_PROCESSES];

void kernel_process_manager_init()
{
    current_process = 0;

    memset(
        process_table,
        0,
        sizeof(process_table)
    );
}

int kernel_process_allocate_pid()
{
    for (int i = 1; i < MAX_PROCESSES; i++) {

        if (!process_table[i])
            return i;
    }

    return -1;
}

void kernel_process_register(
    int pid,
    process_t* process
)
{
    if (
        pid <= 0 ||
        pid >= MAX_PROCESSES
    ) {
        return;
    }

    process_table[pid] = process;
}

process_t* kernel_process_get(
    int pid
)
{
    if (
        pid <= 0 ||
        pid >= MAX_PROCESSES
    ) {
        return 0;
    }

    return process_table[pid];
}

void kernel_process_switch(
    process_t* process
)
{
    current_process = process;
}

process_t* kernel_process_current()
{
    return current_process;
}
