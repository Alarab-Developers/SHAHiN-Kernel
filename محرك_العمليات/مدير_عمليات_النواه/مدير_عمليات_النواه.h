#ifndef KERNEL_PROCESS_MANAGER_H
#define KERNEL_PROCESS_MANAGER_H

#include "محرك_العمليات/محرك_العمليات.h"

extern process_t* current_process;

void kernel_process_manager_init();

int kernel_process_allocate_pid();

void kernel_process_register(
    int pid,
    process_t* process
);

process_t* kernel_process_get(
    int pid
);

void kernel_process_switch(
    process_t* process
);

process_t* kernel_process_current();

#endif
