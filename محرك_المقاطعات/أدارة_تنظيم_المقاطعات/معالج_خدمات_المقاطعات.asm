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

    ; تبدل المكدس
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

; ================================================================
; استثناءات عامة (Divide Error, Invalid Opcode, Double Fault, GPF)
; الهدف: أي كراش من هذا النوع يوصل لـ kernel_panic برسالة واضحة
; بدل ما يسبب triple fault صامت في QEMU
; ================================================================

extern generic_exception_handler_c

isr_exception_common:
    SAVE_REGS
    mov rdi, rsp
    call generic_exception_handler_c
    RESTORE_REGS
    add rsp, 16          ; إزالة vector + error code قبل iretq
    iretq

%macro ISR_NOERR 1
global isr_exc_%1
isr_exc_%1:
    push qword 0         ; error code وهمي (الاستثناء ده مفيهوش error code حقيقي)
    push qword %1        ; رقم الاستثناء (vector)
    jmp isr_exception_common
%endmacro

%macro ISR_ERR 1
global isr_exc_%1
isr_exc_%1:
    push qword %1        ; رقم الاستثناء، فوق error code اللي دفعه المعالج تلقائيًا
    jmp isr_exception_common
%endmacro

ISR_NOERR 0    ; #DE  Divide Error
ISR_NOERR 6    ; #UD  Invalid Opcode
ISR_ERR   8    ; #DF  Double Fault
ISR_ERR   13   ; #GP  General Protection Fault

section .note.GNU-stack noalloc noexec nowrite progbits
