SECTIONS {
  . = 0x0;
  .text : { *(.text) }
  .data : { *(.data) }
  .bss  : { *(.bss) }
  /DISCARD/ : { *(.pdata) *(.xdata) }
}
ENTRY(pop_main)