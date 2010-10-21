/*
 *  linux/arch/or32/kernel/misc.S
 *
 *  or32 version
 *    author(s): Simon Srot (srot@opencores.org)
 *
 *  For more information about OpenRISC processors, licensing and
 *  design services you may contact Beyond Semiconductor at
 *  sales@bsemi.com or visit website http://www.bsemi.com.
 *
 *  derived from cris, i386, m68k, ppc, sh ports.
 *
 *  changes:
 *  18. 11. 2003: Matjaz Breskvar (phoenix@bsemi.com)
 *    initial port to or32 architecture
 *
 */
#include <asm/spr_defs.h>
#include <asm/board.h>
#include <asm/unistd.h>

/* defined in <linux/sched.h> */
#define CLONE_VM  0x00000100

	/*
	 * we could avoid saving of some registers
	 * if we could guarantee certian functions to be inlined
	 */
	
	.text


#if 0

/*
 * Enable interrupts
 *      sti()
 */
	.global __sti
__sti:
	l.addi	r1,r1,-0x4
	l.sw	0x0(r1),r3
	
	l.mfspr	r3,r0,SPR_SR
	l.ori	r3,r3,(SPR_SR_IEE | SPR_SR_TEE)
	l.mtspr	r0,r3,SPR_SR
	l.lwz	r3,0x0(r1)
	l.jr	r9
	l.addi	r1,r1,0x4

/*
 * Disable interrupts
 *      cli()
 */
	.global __cli
__cli:
	l.addi	r1,r1,-0x8
	l.sw	0x0(r1),r4
	l.sw	0x4(r1),r3
	
//	l.sw	-0x4(r1),r4	
	l.addi	r4,r0,-1
	l.xori	r4,r4,(SPR_SR_IEE | SPR_SR_TEE)
	l.mfspr	r3,r0,SPR_SR
	l.and	r3,r3,r4
	l.mtspr	r0,r3,SPR_SR
	
	l.lwz	r4,0x0(r1)
	l.lwz	r3,0x4(r1)
	l.jr	r9
	l.addi	r1,r1,0x8	
//	l.lwz	r4,-0x4(r1)

/*
 * Get 'flags' (aka status register)
 *      save_flags(long *ptr)
 */
	.global __save_flags
__save_flags:
	l.addi	r1,r1,-0x4
	l.sw	0x0(r1),r4
	
//	l.sw	-0x4(r1),r4
	l.mfspr r4,r0,SPR_SR
	l.sw	0(r3),r4

	l.lwz	r4,0x0(r1)
	l.jr	r9
	l.addi	r1,r1,0x4
//	l.lwz	r4,-0x4(r1)

/*
 * Get 'flags' and disable interrupts
 *      save_and_cli(long *ptr)
 */
	.global __save_and_cli
__save_and_cli:
	l.addi	r1,r1,-0x8
	l.sw	0x0(r1),r4
	l.sw	0x4(r1),r3

//	l.sw	-0x4(r1),r4
	l.mfspr r4,r0,SPR_SR
	l.sw	0(r3),r4
	l.addi	r4,r0,-1
	l.xori	r4,r4,(SPR_SR_IEE | SPR_SR_TEE)
	l.mfspr r3,r0,SPR_SR
	l.and	r3,r3,r4
	l.mtspr r0,r3,SPR_SR
	
	l.lwz	r4,0x0(r1)
	l.lwz	r3,0x4(r1)
	l.jr	r9
	l.addi	r1,r1,0x8
//	l.lwz	r4,-0x4(r1)

/*
 * Restore 'flags'
 *      restore_flags(long val)
 */
	.global __restore_flags
__restore_flags:
	l.mtspr r0,r3,SPR_SR
	l.jr	r9
	l.nop


#endif

/*
 * Create a kernel thread
 *   kernel_thread(fn, arg, flags)
 */
  .global kernel_thread
kernel_thread:
	l.addi	r1,r1,-8
	l.sw	0x0(r1),r6
	l.sw	0x4(r1),r3

	/* __PHX__ do we need to save the stat ??? */
	l.add	r6,r0,r3  		/* function */
	l.ori   r3,r5,CLONE_VM		/* flags */
	/* __PHX__ needs also CLONE_UNTRACED */
	l.addi  r11,r0,__NR_clone
	l.sys   1
	l.sfeqi r11,0			/* parent or child? */
	l.bnf   1f			/* return if parent */
	l.nop
	l.addi  r1,r1,-16		/* make top-level stack frame */
	l.sw	0(r1),r0
	l.jalr  r6			/* load arg and call fn */
	l.add	r3,r0,r4
	l.add	r3,r3,r11
	l.addi	r11,r0,__NR_exit	/* exit after child exits */
	l.sys   1
1:
	l.lwz	r6,0x0(r1)
	l.lwz	r3,0x4(r1)
	l.jr    r9
	l.addi	r1,r1,8

/* FIXME: Delete this stuff  */
#if 0 

/*
 * void ic_enable(u32 icache_size, u32 icache_block_size)
 * @icache_size:       size of i-cache
 * @icache_block_size: i-cache block size (line size)
 *
 * Instruction cache enable
 * 
 */
	.global __ic_enable
__ic_enable:
	/* Disable IC */
	l.mfspr r13,r0,SPR_SR
	l.addi  r11,r0,-1
	l.xori  r11,r11,SPR_SR_ICE
	l.and   r11,r13,r11
	l.mtspr r0,r11,SPR_SR

	/* Invalidate IC */
	l.addi  r13,r0,0
	l.add  r11,r0,r3
1:
	l.mtspr r0,r13,SPR_ICBIR
	l.sfne  r13,r11
	l.bf    1b
	l.add  r13,r13,r4

	/* Enable IC */
	l.mfspr r13,r0,SPR_SR
	l.ori   r13,r13,SPR_SR_ICE
	l.mtspr r0,r13,SPR_SR
	l.nop
	l.nop
	l.nop
	l.nop
	l.nop

	l.jr    r9
	l.nop
 
/*
 * Instruction cache disable
 *      ic_disable()
 */
	.global __ic_disable
__ic_disable:
	/* Disable IC */
	l.mfspr r13,r0,SPR_SR
	l.addi  r11,r0,-1
	l.xori  r11,r11,SPR_SR_ICE
	l.and   r11,r13,r11
	l.mtspr r0,r11,SPR_SR

	l.jr    r9
	l.nop
	


/*
 * Instruction cache invalidate
 *      ic_flush()
 */
	.global __ic_invalidate
__ic_invalidate:
	/* Disable IC */
	l.mfspr r13,r0,SPR_SR
	l.addi  r11,r0,-1
	l.xori  r11,r11,SPR_SR_ICE
	l.and   r11,r13,r11
	l.mtspr r0,r11,SPR_SR

	/* Invalidate IC */
	l.addi  r13,r0,0
	l.addi  r11,r0,IC_SIZE
1:
	l.mtspr r0,r13,SPR_ICBIR
	l.sfne  r13,r11
	l.bf    1b
	l.addi  r13,r13,IC_LINE

	/* Enable IC */
	l.mfspr r13,r0,SPR_SR
	l.ori   r13,r13,SPR_SR_ICE
	l.mtspr r0,r13,SPR_SR
	l.nop
	l.nop
	l.nop
	l.nop
	l.nop

	l.jr    r9
	l.nop

/*
 * void __dc_enable(u32 dcache_size, u32 dcache_block_size)
 * @dcache_size:       size of d-cache
 * @dcache_block_size: d-cache block size (line size)
 *
 * Data cache enable
 * 
 */
	.global __dc_enable
__dc_enable:
  /* Disable DC */
	l.mfspr r13,r0,SPR_SR
	l.addi  r11,r0,-1
	l.xori  r11,r11,SPR_SR_DCE
	l.and   r11,r13,r11
	l.mtspr r0,r11,SPR_SR

	/* Flush DC */
	l.addi  r13,r0,0
	l.add  r11,r0,r3
1:
	l.mtspr r0,r13,SPR_DCBIR
	l.sfne  r13,r11
	l.bf    1b
	l.add  r13,r13,r4

	/* Enable DC */
	l.mfspr r13,r0,SPR_SR
	l.ori   r13,r13,SPR_SR_DCE
	l.mtspr r0,r13,SPR_SR

	l.jr    r9
	l.nop
 
/*
 * Data cache disable
 *      dc_disable()
 */
	.global __dc_disable
__dc_disable:
	/* Disable DC */
	l.mfspr r13,r0,SPR_SR
	l.addi  r11,r0,-1
	l.xori  r11,r11,SPR_SR_DCE
	l.and   r11,r13,r11
	l.mtspr r0,r11,SPR_SR

	l.jr    r9
	l.nop
 
/*
 * Invalidate data cache line
 *      dc_line_invalidate(long ph_add)
 */
	.global __dc_line_invalidate
__dc_line_invalidate:
  l.mfspr r4,r0,SPR_SR
  l.addi  r5,r0,-1
  l.xori  r5,r5,SPR_SR_DCE
  l.and   r5,r4,r5
  l.mtspr r0,r5,SPR_SR
  l.mtspr r0,r3,SPR_DCBIR
  l.mtspr r0,r4,SPR_SR
  l.jr    r9
  l.nop

/*
 * Data MMU enable
 *      dmmu_enable()
 */
	.global __dmmu_enable
__dmmu_enable:
  /* Invalidate all sets */
  l.addi  r11,r0,DMMU_SET_NB
  l.addi  r13,r0,0
1:
  l.mtspr r13,r0,SPR_DTLBMR_BASE(0)
  l.addi  r11,r11,-1
  l.sfeqi r11,0
  l.bnf   1b
  l.addi  r13,r13,1
  l.mfspr r11,r0,SPR_SR
  l.ori   r11,r11,SPR_SR_DME
  l.mtspr r0,r11,SPR_SR
  l.jr    r9
  l.nop

/*
 * Instruction MMU enable
 *      immu_enable()
 */
	.global __immu_enable
__immu_enable:
  /* Invalidate all sets */
  l.addi  r11,r0,IMMU_SET_NB
  l.addi  r13,r0,0
1:
  l.mtspr r13,r0,SPR_ITLBMR_BASE(0)
  l.addi  r11,r11,-1
  l.sfeqi r11,0
  l.bnf   1b
  l.addi  r13,r13,1
  l.mfspr r11,r0,SPR_SR
  l.ori   r11,r11,SPR_SR_IME
  l.mtspr r0,r11,SPR_SR
  l.nop
  l.nop
  l.nop
  l.nop
  l.jr    r9
  l.nop
#endif

 /*
 * Print utility
 *      print(const char *fmt, ...)
 */
	.global __print
__print:
	l.lwz	r3,0(r1)
	l.addi	r4,r1,4
#	l.sys	202
  l.nop 3
	l.jr	r9
	l.nop

