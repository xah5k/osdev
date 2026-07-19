ENTRY(AppMain)
SECTIONS
{
    . = 0x0000000000400000;
    .text : {
        KEEP(*(.text)) *(.text .text.*)   /* code */
        *(.rodata .rodata.*)                   /* data */
        *(.data .data.*)
    }
    .bss (NOLOAD) : {                          /* bss */
        . = ALIGN(16);
        *(.bss .bss.*)
        *(COMMON)
    }
    /DISCARD/ : { *(.eh_frame) *(.comment) }
}