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
#  define LOAD_OFFSET  0x00000000
#  define LOAD_BASE    0x00000000
#endif

#include <asm-generic/vmlinux.lds.h>

	
OUTPUT_FORMAT("elf32-or32", "elf32-or32", "elf32-or32")
OUTPUT_ARCH(or32)
_jiffies = _jiffies_64 + 4;
SECTIONS
{
        /* Read-only sections, merged into text segment: */
        . = LOAD_BASE ; 
        _dram_start = .;

        __s_protected_core = .;
	
        __stext = .;
        .text			: AT(ADDR(.text) - LOAD_OFFSET)
        {
          *(.text)
          SCHED_TEXT
	  LOCK_TEXT
          *(.fixup)
          *(.text.__*)
        }
        __etext = .;

        RODATA
	
        __e_protected_core = .;

	. = ALIGN(4); 
	__ex_table		: AT(ADDR(__ex_table) - LOAD_OFFSET)
	{
		___start___ex_table = .;
		*(__ex_table)
		___stop___ex_table = .;
	}

        . = ALIGN(4);
        .data    		: AT(ADDR(.data) - LOAD_OFFSET)
        {
          *(.data)
        }

	. = ALIGN(32);
	.data.cacheline_aligned	: AT(ADDR(.data.cacheline_aligned) - LOAD_OFFSET)
	{ 
		*(.data.cacheline_aligned) 
	}
	
        __edata  =  .;

       /* init_task and stack, must be aligned */
        . = ALIGN(8192);      
        .data.init_task		: AT(ADDR(.data.init_task) - LOAD_OFFSET) 
        { 
          *(.data.init_task) 
        }
 
        /* Init code and data */
        . = ALIGN(8192);
        ___init_begin = .;
        .init.text		: AT(ADDR(.init.text) - LOAD_OFFSET)
	{
		__sinittext = .;
                *(.init.text)
                __einittext = .;
        }

        .init.data		: AT(ADDR(.init.data) - LOAD_OFFSET)
	{ 
		*(.init.data) 
		. = ALIGN(8);
                VMLINUX_SYMBOL(__ctors_start) = .;
                *(.ctors)
                VMLINUX_SYMBOL(__ctors_end) = .;
	}

	/* Setup calls */
        . = ALIGN(16);
        ___setup_start = .;
        .init.setup		: AT(ADDR(.init.setup) - LOAD_OFFSET) 
	{ 
		*(.init.setup) 
	}
        ___setup_end = .;

	
        ___start___param = .;
        __param			: AT(ADDR(__param) - LOAD_OFFSET) 
	{ 
		*(__param) 
	}
        ___stop___param = .;


	
        .initcall.init		: AT(ADDR(.initcall.init) - LOAD_OFFSET)
	{
                ___initcall_start = .;
                INITCALLS
                ___initcall_end = .;
        }

        .con_initcall.init	: AT(ADDR(.con_initcall.init) - LOAD_OFFSET) 
	{
                ___con_initcall_start = .;
                *(.con_initcall.init)
                ___con_initcall_end = .;
        }
	
        SECURITY_INIT


	. = ALIGN(8192); 
	.init.ramfs		: AT(ADDR(.init.ramfs) - LOAD_OFFSET)
	{
                ___initramfs_start = .;
                *(.init.ramfs)
                ___initramfs_end = .;
                /* We fill to the next page, so we can discard all init
                   pages without needing to consider what payload might be
                   appended to the kernel image.  */
                FILL (0);
                . = ALIGN (8192);
        }
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
/*        ___init_end = .;
*/
        ___data_end = . ;              /* Move to _edata ? */
        ___bss_start = .;              /* BSS */
        .bss			: AT(ADDR(.bss) - LOAD_OFFSET)
	{
                *(COMMON)
                *(.bss)
        }

        . =  ALIGN (0x20);
        __end = .;

        /* Sections to be discarded */
        /DISCARD/ : 
	{
		*(.exit.text)
                *(.text.exit)
                *(.data.exit)
                *(.exitcall.exit)
        }

	/* we need to add an extra underscore */
	PROVIDE(___start___ksymtab	= __start___ksymtab); 
	PROVIDE(___stop___ksymtab	= __stop___ksymtab);
	PROVIDE(___start___ksymtab_gpl	= __start___ksymtab_gpl);
	PROVIDE(___stop___ksymtab_gpl	= __stop___ksymtab_gpl);
	PROVIDE(___sched_text_start     = __sched_text_start);
	PROVIDE(___sched_text_end       = __sched_text_end);
}
