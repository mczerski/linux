/* ld script to make Linux/or32 kernel
 * Authors: Matjaz Breskvar <phoenix@bsemi.com>
 *
 *  For more information about OpenRISC processors, licensing and
 *  design services you may contact Beyond Semiconductor at
 *  sales@bsemi.com or visit website http://www.bsemi.com.
 */

/* __PHX__ :: TODO 
 *		- clean up __offset & stuff
 *		- change all 8192 aligment to PAGE !!!
 *		- recheck if all aligments are really needed
 */

#ifdef CONFIG_OPENRISC_FLASH_BOOT
#  define LOAD_OFFSET -0x30000000
#  define LOAD_BASE    0xc0000000
#else
#  define LOAD_OFFSET  PAGE_OFFSET
#  define LOAD_BASE    PAGE_OFFSET
#endif

#include <asm/page.h>
#include <asm-generic/vmlinux.lds.h>
	
OUTPUT_FORMAT("elf32-or32", "elf32-or32", "elf32-or32")
/*OUTPUT_ARCH(or32)*/
jiffies = jiffies_64 + 4;
/*PAGE_SIZE = 8192;*/

SECTIONS
{
        /* Read-only sections, merged into text segment: */
        . = LOAD_BASE ; 

	
        .text                   : AT(ADDR(.text) - LOAD_OFFSET)
	{
          _stext = .;
	  TEXT_TEXT
	  SCHED_TEXT
	  LOCK_TEXT
	  KPROBES_TEXT
	  IRQENTRY_TEXT
	  *(.fixup)
	  *(.text.__*)
	  _etext = .;
	}
	
	/* FIXME: This should include the above .text bits but that currently
	 * doesn't work... something's writing in there...
	 */
	_s_kernel_ro = .;

	/* TODO: Check if fixup and text.__* are really necessary */

	. = ALIGN (4) ;
        _fdt_blob : AT(ADDR(_fdt_blob) - LOAD_OFFSET) {
                _fdt_start = . ;                /* place for fdt blob */
                *(_fdt_blob) ;                 /* Any link-placed DTB */
                . = _fdt_start + 0x4000;        /* Pad up to 16kbyte */
                _fdt_end = . ;
        }

	EXCEPTION_TABLE(4)

	_sdata = .;

	/* Jonas --> I'm making this page aligned, was 4096 */
	/* TODO: NOTE THAT RODATA was inside e_protected_core before... is this necessary? */
	RO_DATA_SECTION(8192)

	_e_kernel_ro = .;

	/* 32 here is cacheline size... recheck this <-- Jonas */
	RW_DATA_SECTION(32, 8192, 8192)

        _edata  =  .;

	/* Init code and data */
	. = ALIGN(8192);
	__init_begin = .;

	HEAD_TEXT_SECTION

	/* Page aligned */
	INIT_TEXT_SECTION(8192)

	/* Align __setup_start on 16 byte boundary */
	INIT_DATA_SECTION(16)

	PERCPU(8192)

        __init_end = .;
	
	. = ALIGN(8192); 
	.initrd			: AT(ADDR(.initrd) - LOAD_OFFSET)
	{
		__initrd_start = .; 
		*(.initrd)
		__initrd_end = .; 
		FILL (0);
                . = ALIGN (8192);
	}

        __vmlinux_end = .;            /* last address of the physical file */

	BSS_SECTION(0, 0, 0x20)

        _end = .;

	/* Throw in the debugging sections */
	STABS_DEBUG
	DWARF_DEBUG

        /* Sections to be discarded -- must be last */
	DISCARDS
}
