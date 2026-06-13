bits 64
global load_idt

; void load_idt(uint64_t idt_ptr_address)
; rdi = عنوان idt_ptr_t (limit + base)
load_idt:
    lidt [rdi]
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
