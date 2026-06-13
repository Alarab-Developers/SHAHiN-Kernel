bits 64

global isr_timer
global isr_keyboard
global isr_mouse
global isr_page_fault

extern irq_handler


extern page_fault_handler_c

%macro SAVE_REGS 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro RESTORE_REGS 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

; ================================================================


extern process_on_timer_interrupt

isr_timer:
    SAVE_REGS

    mov rdi, 0
    call irq_handler

    mov rdi, rsp
    call process_on_timer_interrupt

    mov rsp, rax

    RESTORE_REGS
    iretq

; ================================================================
; isr_keyboard — IRQ1 → vector 33
; ================================================================
isr_keyboard:
    SAVE_REGS
    mov rdi, 1
    call irq_handler
    RESTORE_REGS
    iretq

; ================================================================
; isr_mouse — IRQ12 → vector 44
; ================================================================
isr_mouse:
    SAVE_REGS
    mov rdi, 12
    call irq_handler
    RESTORE_REGS
    iretq

; ================================================================
; isr_page_fault — interrupt 14
; ================================================================
isr_page_fault:
    SAVE_REGS
    mov rdi, rsp
    call page_fault_handler_c
    RESTORE_REGS
    add rsp, 8
    iretq

section .note.GNU-stack noalloc noexec nowrite progbits
