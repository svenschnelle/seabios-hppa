#include "parisc/hppa_hardware.h"

OUTPUT_FORMAT("elf32-hppa-linux")
OUTPUT_ARCH(hppa)
ENTRY(startup)
SECTIONS
{
	. = FIRMWARE_START;

	/* align on next page boundary */
	. = ALIGN(4096);
	.text :	{
		_text = .;	/* Text */
		*(.head.text)
                . = ALIGN(4096);
                *(.text.sti.header)
                *(.text.sti.regionlist)
		*(.text)
                *(.text.stifunc.*)
                . = ALIGN(4096);
                *(.text.sti.end)
		*(.text.*)
		_etext = . ;
	}
	. = ALIGN(8);
	.rodata : {
		_rodata = . ;
		*(.rodata)	 /* read-only data */
		*(.rodata.*)
		_erodata = . ;
	}
	. = ALIGN(8);
	.data :	{
		_data = . ;
		*(.data)
		*(.data.*)
		_edata = . ;
	}
	. = ALIGN(8);
	.bss : {
		_bss = . ;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		_ebss = .;
	}

	/* STABS_DEBUG */
	.note 0 : { *(.note) }

	/* Sections to be discarded */
	/DISCARD/ : {
	}
}
