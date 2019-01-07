OUTPUT_ARCH( "riscv" )
ENTRY(_start)

SECTIONS
{
  . = 0x100000;
  .text : { *(.text) }
  . = ALIGN(0x400);
  .data : { *(.data) }
  .bss : { *(.bss) }
  _end = .;
}

