ENTRY(AppMain)
SECTIONS
{
    . = ;
    .text : {
        KEEP(*(.text.boot)) *(.text .text.*)   /* code */
        *(.rodata .rodata.*)                   /* data */
        *(.data .data.*)
    } :boot
    .bss (NOLOAD) : {                          /* bss */
        . = ALIGN(16);
        *(.bss .bss.*)
        *(COMMON)
    } :boot
    /DISCARD/ : { *(.eh_frame) *(.comment) }
}