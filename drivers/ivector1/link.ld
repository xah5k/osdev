ENTRY(DriverEntry);
SECTIONS
{
  /* Combine all .text sections into one .text section */
  .text :
  {
    *(.text)
    *(.text.*)
  }

  /* Combine all read-only data sections */
  .rodata :
  {
    *(.rodata)
    *(.rodata.*)
  }

  /* Combine all initialized data sections */
  .data :
  {
    *(.data)
    *(.data.*)
  }

  /* Combine all uninitialized data sections */
  .bss :
  {
    *(.bss)
    *(.bss.*)
  }
}