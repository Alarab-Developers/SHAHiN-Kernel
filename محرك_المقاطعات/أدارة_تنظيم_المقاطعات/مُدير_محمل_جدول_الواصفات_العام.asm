bits 64

global load_gdt

; void load_gdt(uint64_t gdt_ptr_address)
; rdi = عنوان gdt_ptr_t (limit + base)
load_gdt:
    lgdt [rdi]

    mov ax, 0x10        ; selector البيانات (Data Segment)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    pop rax              ; عنوان العودة (وضعه call تلقائياً على المكدس)
    push 0x08             ; CS الجديد (Code Segment)
    push rax              ; RIP للعودة
    retfq                 ; يعيد تحميل CS=0x08 ثم يرجع لنفس نقطة الاستدعاء

section .note.GNU-stack noalloc noexec nowrite progbits
