ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 12K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Stack_Size = 0x400;

SECTIONS
{
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
    .text :
    {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH
    .ARM.extab : { *(.ARM.extab*) } > FLASH
    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH
    .preinit_array :
    {
        . = ALIGN(4);
        __preinit_array_start = .;
        KEEP(*(.preinit_array*))
        __preinit_array_end = .;
    } > FLASH
    .init_array :
    {
        . = ALIGN(4);
        __init_array_start = .;
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    } > FLASH
    .fini_array : { KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array)) } > FLASH
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT> FLASH
    _sidata = LOADADDR(.data);
    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*) *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM
    .noinit (NOLOAD) :
    {
        . = ALIGN(4);
        *(.noinit*)
        . = ALIGN(8);
        _end = .;
    } > RAM
    ASSERT(_end <= _estack - _Min_Stack_Size, "RAM leaves less than 1 KB for stack")
}
