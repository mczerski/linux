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
 *		- make ramsize an option (maybe config option) or suck it of
 * hardware and calculate dram_end acordingly
 */

#ifdef CONFIG_OR32_FLASH_BOOT
#  define LOAD_OFFSET -0x30000000
#  define LOAD_BASE    0xc0000000
#else
#  define LOAD_OFFSET  PAGE_OFFSET
#  define LOAD_BASE    PAGE_OFFSET
#endif

#include <asm/page.h>
#include <asm-generic/vmlinux.lds.h>
	
OUTPUT_FORMAT("elf32-or32", "elf32-or32", "elf32-or32")
OUTPUT_ARCH(or32)
_jiffies = _jiffies_64 + 4;
/*PAGE_SIZE = 8192;*/

SECTIONS
{
        /* Read-only sections, merged into text segment: */
        . = LOAD_BASE ; 
        _dram_start = .;

        __s_protected_core = .;
	
        .text                   : AT(ADDR(.text) - LOAD_OFFSET)
	{
          __stext = .;
	  TEXT_TEXT
	  SCHED_TEXT
	  LOCK_TEXT
	  KPROBES_TEXT
	  IRQENTRY_TEXT
	  *(.fixup)
	  *(.text.__*)
	  __etext = .;
	}

	/* TODO: Check if fixup and text.__* are really necessary */

        __e_protected_core = .;

	EXCEPTION_TABLE(4)

	__sdata = .;

	/* Jonas --> I'm making this page aligned, was 4096 */
	/* TODO: NOTE THAT RODATA was inside e_protected_core before... is this necessary? */
	RO_DATA_SECTION(8192)
	/* 32 here is cacheline size... recheck this <-- Jonas */
	RW_DATA_SECTION(32, 8192, 8192)

        __edata  =  .;
 
        /* Init code and data */
        . = ALIGN(8192);
        ___init_begin = .;

	HEAD_TEXT_SECTION

	/* Page aligned */
	INIT_TEXT_SECTION(8192)

	/* Align ___setup_start on 16 byte boundary */
	INIT_DATA_SECTION(16)

	. = ALIGN(8192);

	/* TODO: PERCPU sections should go here */

        ___init_end = .;
	
	. = ALIGN(8192); 
	.initrd			: AT(ADDR(.initrd) - LOAD_OFFSET)
	{
		___initrd_start = .; 
		*(.initrd)
		FILL (0);
                . = ALIGN (8192);
		___initrd_end = .; 
	}

        ___vmlinux_end = .;            /* last address of the physical file */
        ___data_end = . ;              /* Move to _edata ? */


	BSS_SECTION(0, 0, 0x20)

        __end = .;

        /* Sections to be discarded */

	DISCARDS

	/* Throw in the debugging sections */
	STABS_DEBUG
	DWARF_DEBUG
}
