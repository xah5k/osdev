ENTRY(_start)

PHDRS
{
    limine_requests PT_LOAD;
    text PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
}

SECTIONS
{
    /* keeping old base */
    . = 0xffffffffffe02000;
    _kernel_start = .;
    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests
    . = ALIGN(CONSTANT(MAXPAGESIZE));
    .text : {
        *(.text .text.*)
    } :text
    . = ALIGN(CONSTANT(MAXPAGESIZE));
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata
    .note.gnu.build-id : {
        *(.note.gnu.build-id)
    } :rodata
    . = ALIGN(CONSTANT(MAXPAGESIZE));
    .data : {
        *(.data .data.*)
    } :data
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :data
    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
    }
    .kexports : ALIGN(8) {
        __start_kexports = .;
        KEEP(*(.kexports))
        __end_kexports = .;
    }
    _kernel_end = .;
}