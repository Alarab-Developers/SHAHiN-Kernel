ENTRY(KernelMain)

SECTIONS
{
    /* تحميل النواة عند 1MB */
    . = 0x100000;

    . = ALIGN(4096);

    .text :
    {
        *(.text*)
    }

    . = ALIGN(4096);

    .rodata :
    {
        *(.rodata*)
    }

    . = ALIGN(4096);

    .data :
    {
        *(.data*)
    }

    . = ALIGN(4096);

    .bss :
    {
        *(COMMON)
        *(.bss*)
    }

    _kernel_start = ADDR(.text);
    _kernel_end   = .;

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.comment)
        *(.note*)
    }
}
