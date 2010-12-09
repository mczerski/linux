/*
 *  linux/arch/or32/kernel/entry.S
 *
 *  or32 version
 *    author(s): Matjaz Breskvar (phoenix@bsemi.com)
 *
 *  For more information about OpenRISC processors, licensing and
 *  design services you may contact Beyond Semiconductor at
 *  sales@bsemi.com or visit website http://www.bsemi.com.
 *
 *  changes:
 *  18. 11. 2003: Matjaz Breskvar (phoenix@bsemi.com)
 *    initial port to or32 architecture
 *  06. 12. 2003: Matjaz Breskvar (phoenix@bsemi.com)
 *    major changes in handling exceptions
 *    physical addresses used only by code in head.S
 *    cleand exception handlers and moved them to entry.S
 *  31. 12. 2005: Gyorgy Jeney (nog@bsemi.com)
 *    Added syscall traceing code
 *
 * Stack layout in 'ret_from_system_call':
 *      ptrace needs to have all regs on the stack.
 *      if the order here is changed, it needs to be
 *      updated in fork.c:copy_process, signal.c:do_signal,
 *      ptrace.c and ptrace.h
 *
 */

#include <linux/linkage.h>
#include <linux/sys.h>
#include <asm/processor.h>
#include <asm/thread_info.h>
#include <asm/unistd.h>
#include <asm/errno.h>
#include <asm/spr_defs.h>

#include <asm/page.h>
#include <asm/mmu.h>
#include <asm/pgtable.h>
#include <asm/asm-offsets.h>
		
/* some defines to ease or32 assembly programming */
#include "or32_funcs.S"
	
/* =========================================================[ macros ]=== */

#define PUSH_PT_REGS_NO_TRANS					\
        l.addi  r1,r1,-(INT_FRAME_SIZE)				;\
        l.sw    GPR3(r1),r3					;\
        l.sw    ORIG_GPR3(r1),r3				;\
        l.mfspr r3,r0,SPR_EPCR_BASE				;\
        l.sw    PC(r1),r3					;\
        l.mfspr r3,r0,SPR_ESR_BASE				;\
        l.sw    SR(r1),r3					;\
        l.addi  r3,r1,+(INT_FRAME_SIZE)				;\
        l.sw    SP(r1),r3					;\
        l.sw    GPR2(r1),r2					;\
	l.lwz   r3,GPR3(r1)					;\
	l.sw    GPR4(r1),r4					;\
        l.sw    GPR5(r1),r5					;\
        l.sw    GPR6(r1),r6					;\
        l.sw    GPR7(r1),r7					;\
        l.sw    GPR8(r1),r8					;\
        l.sw    GPR9(r1),r9					;\
        l.sw    GPR10(r1),r10					;\
        l.sw    GPR11(r1),r11					;\
        l.sw    GPR12(r1),r12					;\
        l.sw    GPR13(r1),r13					;\
        l.sw    GPR14(r1),r14					;\
        l.sw    GPR15(r1),r15					;\
        l.sw    GPR16(r1),r16					;\
        l.sw    GPR17(r1),r17					;\
        l.sw    GPR18(r1),r18					;\
        l.sw    GPR19(r1),r19					;\
        l.sw    GPR20(r1),r20					;\
        l.sw    GPR21(r1),r21					;\
        l.sw    GPR22(r1),r22					;\
        l.sw    GPR23(r1),r23					;\
        l.sw    GPR24(r1),r24					;\
        l.sw    GPR25(r1),r25					;\
        l.sw    GPR26(r1),r26					;\
        l.sw    GPR27(r1),r27					;\
        l.sw    GPR28(r1),r28					;\
        l.sw    GPR29(r1),r29					;\
        l.sw    GPR30(r1),r30					;\
        l.sw    GPR31(r1),r31					;\
        l.sw    RESULT(r1),r0

	
/*
 * We need to disable interrupts at beginning of RESTORE_ALL
 * since interrupt might come in after we've loaded EPC return address
 * and overwrite EPC with address somewhere in RESTORE_ALL
 * which is of course wrong!
 */
	
#define RESTORE_ALL						\
	DISABLE_INTERRUPTS(r3,r4)				;\
	l.lwz   r3,PC(r1)					;\
        l.mtspr r0,r3,SPR_EPCR_BASE				;\
        l.lwz   r3,SR(r1)					;\
        l.mtspr r0,r3,SPR_ESR_BASE				;\
        l.lwz   r2,GPR2(r1)					;\
        l.lwz   r3,GPR3(r1)					;\
        l.lwz   r4,GPR4(r1)					;\
        l.lwz   r5,GPR5(r1)					;\
        l.lwz   r6,GPR6(r1)					;\
        l.lwz   r7,GPR7(r1)					;\
        l.lwz   r8,GPR8(r1)					;\
        l.lwz   r9,GPR9(r1)					;\
        l.lwz   r10,GPR10(r1)					;\
        l.lwz   r11,GPR11(r1)					;\
        l.lwz   r12,GPR12(r1)					;\
        l.lwz   r13,GPR13(r1)					;\
        l.lwz   r14,GPR14(r1)					;\
        l.lwz   r15,GPR15(r1)					;\
        l.lwz   r16,GPR16(r1)					;\
        l.lwz   r17,GPR17(r1)					;\
        l.lwz   r18,GPR18(r1)					;\
        l.lwz   r19,GPR19(r1)					;\
        l.lwz   r20,GPR20(r1)					;\
        l.lwz   r21,GPR21(r1)					;\
        l.lwz   r22,GPR22(r1)					;\
        l.lwz   r23,GPR23(r1)					;\
        l.lwz   r24,GPR24(r1)					;\
        l.lwz   r25,GPR25(r1)					;\
        l.lwz   r26,GPR26(r1)					;\
        l.lwz   r27,GPR27(r1)					;\
        l.lwz   r28,GPR28(r1)					;\
        l.lwz   r29,GPR29(r1)					;\
        l.lwz   r30,GPR30(r1)					;\
        l.lwz   r31,GPR31(r1)					;\
        l.lwz   r1,SP(r1)					;\
	l.rfe

#define RESTORE_ALL_NO_R11					\
	DISABLE_INTERRUPTS(r3,r4)				;\
	l.lwz   r3,PC(r1)					;\
        l.mtspr r0,r3,SPR_EPCR_BASE				;\
        l.lwz   r3,SR(r1)					;\
        l.mtspr r0,r3,SPR_ESR_BASE				;\
        l.lwz   r2,GPR2(r1)					;\
        l.lwz   r3,GPR3(r1)					;\
        l.lwz   r4,GPR4(r1)					;\
        l.lwz   r5,GPR5(r1)					;\
        l.lwz   r6,GPR6(r1)					;\
        l.lwz   r7,GPR7(r1)					;\
        l.lwz   r8,GPR8(r1)					;\
        l.lwz   r9,GPR9(r1)					;\
        l.lwz   r10,GPR10(r1)					;\
        l.lwz   r12,GPR12(r1)					;\
        l.lwz   r13,GPR13(r1)					;\
        l.lwz   r14,GPR14(r1)					;\
        l.lwz   r15,GPR15(r1)					;\
        l.lwz   r16,GPR16(r1)					;\
        l.lwz   r17,GPR17(r1)					;\
        l.lwz   r18,GPR18(r1)					;\
        l.lwz   r19,GPR19(r1)					;\
        l.lwz   r20,GPR20(r1)					;\
        l.lwz   r21,GPR21(r1)					;\
        l.lwz   r22,GPR22(r1)					;\
        l.lwz   r23,GPR23(r1)					;\
        l.lwz   r24,GPR24(r1)					;\
        l.lwz   r25,GPR25(r1)					;\
        l.lwz   r26,GPR26(r1)					;\
        l.lwz   r27,GPR27(r1)					;\
        l.lwz   r28,GPR28(r1)					;\
        l.lwz   r29,GPR29(r1)					;\
        l.lwz   r30,GPR30(r1)					;\
        l.lwz   r31,GPR31(r1)					;\
        l.lwz   r1,SP(r1)					;\
	l.rfe

		
#define EXCEPTION_ENTRY(handler)				\
	.global	handler						;\
handler:							;\
        /* r1, EPCR, ESR a already saved */			;\
	l.sw	GPR2(r1),r2					;\
	l.sw    GPR3(r1),r3					;\
        l.sw    ORIG_GPR3(r1),r3				;\
	/* r4 already save */					;\
        l.sw    GPR5(r1),r5					;\
        l.sw    GPR6(r1),r6					;\
        l.sw    GPR7(r1),r7					;\
        l.sw    GPR8(r1),r8					;\
        l.sw    GPR9(r1),r9					;\
	/* r10 already saved */					;\
        l.sw    GPR11(r1),r11					;\
	/* r12,r13 already saved */				;\
        l.sw    GPR14(r1),r14					;\
        l.sw    GPR15(r1),r15					;\
        l.sw    GPR16(r1),r16					;\
        l.sw    GPR17(r1),r17					;\
        l.sw    GPR18(r1),r18					;\
        l.sw    GPR19(r1),r19					;\
        l.sw    GPR20(r1),r20					;\
        l.sw    GPR21(r1),r21					;\
        l.sw    GPR22(r1),r22					;\
        l.sw    GPR23(r1),r23					;\
        l.sw    GPR24(r1),r24					;\
        l.sw    GPR25(r1),r25					;\
        l.sw    GPR26(r1),r26					;\
        l.sw    GPR27(r1),r27					;\
        l.sw    GPR28(r1),r28					;\
        l.sw    GPR29(r1),r29					;\
        l.sw    GPR30(r1),r30					;\
	/* r31 already save */					;\
        l.sw    RESULT(r1),r0

#define UNHANDLED_EXCEPTION(handler,vector)			\
	.global	handler						;\
handler:							;\
        /* r1, EPCR, ESR already saved */			;\
	l.sw    GPR2(r1),r2					;\
	l.sw    GPR3(r1),r3					;\
        l.sw    ORIG_GPR3(r1),r3				;\
        l.sw    GPR5(r1),r5					;\
        l.sw    GPR6(r1),r6					;\
        l.sw    GPR7(r1),r7					;\
        l.sw    GPR8(r1),r8					;\
        l.sw    GPR9(r1),r9					;\
	/* r10 already saved */					;\
        l.sw    GPR11(r1),r11					;\
	/* r12,r13 already saved */				;\
        l.sw    GPR14(r1),r14					;\
        l.sw    GPR15(r1),r15					;\
        l.sw    GPR16(r1),r16					;\
        l.sw    GPR17(r1),r17					;\
        l.sw    GPR18(r1),r18					;\
        l.sw    GPR19(r1),r19					;\
        l.sw    GPR20(r1),r20					;\
        l.sw    GPR21(r1),r21					;\
        l.sw    GPR22(r1),r22					;\
        l.sw    GPR23(r1),r23					;\
        l.sw    GPR24(r1),r24					;\
        l.sw    GPR25(r1),r25					;\
        l.sw    GPR26(r1),r26					;\
        l.sw    GPR27(r1),r27					;\
        l.sw    GPR28(r1),r28					;\
        l.sw    GPR29(r1),r29					;\
        l.sw    GPR30(r1),r30					;\
	/* r31 already saved */					;\
        l.sw    RESULT(r1),r0					;\
	l.addi	r3,r1,0						;\
	/* r4 is exception EA */				;\
	l.addi	r5,r0,vector					;\
	l.jal	unhandled_exception				;\
	l.nop							;\
	l.j	_ret_from_exception				;\
	l.nop	1
	
/*
 * NOTE: one should never assume that SPR_EPC, SPR_ESR, SPR_EEAR
 *       contain the same values as when exception we're handling
 *	 occured. in fact they never do. if you need them use
 *	 values saved on stack (for SPR_EPC, SPR_ESR) or content
 *       of r4 (for SPR_EEAR). for details look at EXCEPTION_HANDLE()
 *       in 'arch/or32/kernel/head.S'
 */
	
/* =====================================================[ exceptions] === */

/* ---[ 0x100: RESET exception ]----------------------------------------- */

EXCEPTION_ENTRY(_tng_kernel_start)
	l.jal	_start
	l.nop
	
/* ---[ 0x200: BUS exception ]------------------------------------------- */
	
UNHANDLED_EXCEPTION(_vector_0x200,0x200)

/* ---[ 0x300: Data Page Fault exception ]------------------------------- */

EXCEPTION_ENTRY(_data_page_fault_handler)
	/* set up parameters for do_page_fault */
	l.addi  r3,r1,0                    // pt_regs
	/* r4 set be EXCEPTION_HANDLE */   // effective address of fault
	l.ori   r5,r0,0x300                // exception vector

	/*
	 * __PHX__: TODO
	 *
	 * all this can be written much simpler. look at
	 * DTLB miss handler in the CONFIG_GUARD_PROTECTED_CORE part
	 */
#ifdef CONFIG_OPENRISC_NO_SPR_SR_DSX
	l.lwz   r6,PC(r3)                  // address of an offending insn
        l.lwz   r6,0(r6)                   // instruction that caused pf

        l.srli  r6,r6,26                   // check opcode for jump insn
        l.sfeqi r6,0                       // l.j
        l.bf    8f
        l.sfeqi r6,1                       // l.jal
        l.bf    8f
        l.sfeqi r6,3                       // l.bnf
        l.bf    8f
        l.sfeqi r6,4                       // l.bf
        l.bf    8f
        l.sfeqi r6,0x11                    // l.jr
        l.bf    8f
        l.sfeqi r6,0x12                    // l.jalr
        l.bf    8f
	
        l.nop

        l.j     9f
        l.nop
8:

        l.lwz   r6,PC(r3)                  // address of an offending insn
        l.addi  r6,r6,4
        l.lwz   r6,0(r6)                   // instruction that caused pf
        l.srli  r6,r6,26                   // get opcode
9:

#else

	l.mfspr r6,r0,SPR_SR		   // SR
//	l.lwz	r6,SR(r3)		   // ESR
	l.andi	r6,r6,SPR_SR_DSX	   // check for delay slot exception
	l.sfeqi	r6,0x1			   // exception happened in delay slot
	l.bnf	7f	
	l.lwz	r6,PC(r3)		   // address of an offending insn
	
	l.addi	r6,r6,4			   // offending insn is in delay slot
7:
	l.lwz   r6,0(r6)                   // instruction that caused pf
	l.srli  r6,r6,26                   // check opcode for write access			
#endif		

	l.sfgeui r6,0x34		   // check opcode for write access
	l.bnf   1f
	l.sfleui r6,0x37
	l.bnf   1f
	l.ori   r6,r0,0x1                  // write access
	l.j     2f
	l.nop
1:	l.ori   r6,r0,0x0                  // !write access
2:
	
	/* call fault.c handler in or32/mm/fault.c */
	l.jal   do_page_fault
	l.nop
	l.j     _ret_from_exception
	l.nop
	
/* ---[ 0x400: Insn Page Fault exception ]------------------------------- */

EXCEPTION_ENTRY(_insn_page_fault_handler)	
	/* set up parameters for do_page_fault */
	l.addi  r3,r1,0                    // pt_regs
	/* r4 set be EXCEPTION_HANDLE */   // effective address of fault
	l.ori   r5,r0,0x400                // exception vector
	l.ori	r6,r0,0x0		   // !write access
	
	/* call fault.c handler in or32/mm/fault.c */
	l.jal   do_page_fault
	l.nop
	l.j     _ret_from_exception
	l.nop

	
/* ---[ 0x500: Timer exception ]----------------------------------------- */

EXCEPTION_ENTRY(_timer_handler)
        l.jal	timer_interrupt
	 l.addi r3,r1,0 /* pt_regs */

	l.j    _ret_from_intr
	 l.nop

/* ---[ 0x600: Aligment exception ]-------------------------------------- */

EXCEPTION_ENTRY(_alignment_handler)
	/* r4: EA of fault (set by EXCEPTION_HANDLE) */
	l.jal   do_unaligned_access
	 l.addi  r3,r1,0 /* pt_regs */

	l.j     _ret_from_exception
	 l.nop
		
#if 0
EXCEPTION_ENTRY(_aligment_handler)
//        l.mfspr r2,r0,SPR_EEAR_BASE     /* Load the efective addres */
	l.addi	r2,r4,0
//        l.mfspr r5,r0,SPR_EPCR_BASE     /* Load the insn address */
	l.lwz   r5,PC(r1)
	
        l.lwz   r3,0(r5)                /* Load insn */
        l.srli  r4,r3,26                /* Shift left to get the insn opcode */

        l.sfeqi r4,0x00                 /* Check if the load/store insn is in delay slot */
        l.bf    jmp
        l.sfeqi r4,0x01
        l.bf    jmp
        l.sfeqi r4,0x03
        l.bf    jmp
        l.sfeqi r4,0x04
        l.bf    jmp
        l.sfeqi r4,0x11
        l.bf    jr
        l.sfeqi r4,0x12
        l.bf    jr
        l.nop
        l.j     1f
        l.addi  r5,r5,4                 /* Increment PC to get return insn address */
        
jmp:
        l.slli  r4,r3,6                 /* Get the signed extended jump length */
        l.srai  r4,r4,4

        l.lwz   r3,4(r5)                /* Load the real load/store insn */

        l.add   r5,r5,r4                /* Calculate jump target address */
        
        l.j     1f
        l.srli  r4,r3,26                /* Shift left to get the insn opcode */

jr:
        l.slli  r4,r3,9                 /* Shift to get the reg nb */
        l.andi  r4,r4,0x7c

        l.lwz   r3,4(r5)                /* Load the real load/store insn */

        l.add   r4,r4,r1                /* Load the jump register value from the stack */
        l.lwz   r5,0(r4)

        l.srli  r4,r3,26                /* Shift left to get the insn opcode */
        
        
1:      
//	  l.mtspr r0,r5,SPR_EPCR_BASE
	l.sw	PC(r1),r5
	
        l.sfeqi r4,0x26
        l.bf    lhs
        l.sfeqi r4,0x25
        l.bf    lhz
        l.sfeqi r4,0x22
        l.bf    lws
        l.sfeqi r4,0x21
        l.bf    lwz
        l.sfeqi r4,0x37
        l.bf    sh
        l.sfeqi r4,0x35
        l.bf    sw
        l.nop

1:      l.j     1b                      /* I don't know what to do */
        l.nop

lhs:    l.lbs   r5,0(r2)
        l.slli  r5,r5,8
        l.lbz   r6,1(r2)
        l.or    r5,r5,r6
        l.srli  r4,r3,19
        l.andi  r4,r4,0x7c
        l.add   r4,r4,r1
        l.j     align_end
        l.sw    0(r4),r5        
        
lhz:    l.lbz   r5,0(r2)
        l.slli  r5,r5,8
        l.lbz   r6,1(r2)
        l.or    r5,r5,r6
        l.srli  r4,r3,19
        l.andi  r4,r4,0x7c
        l.add   r4,r4,r1
        l.j     align_end
        l.sw    0(r4),r5        
                
lws:    l.lbs   r5,0(r2)
        l.slli  r5,r5,24
        l.lbz   r6,1(r2)
        l.slli  r6,r6,16
        l.or    r5,r5,r6
        l.lbz   r6,2(r2)
        l.slli  r6,r6,8
        l.or    r5,r5,r6
        l.lbz   r6,3(r2)
        l.or    r5,r5,r6
        l.srli  r4,r3,19
        l.andi  r4,r4,0x7c
        l.add   r4,r4,r1
        l.j     align_end
        l.sw    0(r4),r5        
                
lwz:    l.lbz   r5,0(r2)
        l.slli  r5,r5,24
        l.lbz   r6,1(r2)
        l.slli  r6,r6,16
        l.or    r5,r5,r6
        l.lbz   r6,2(r2)
        l.slli  r6,r6,8
        l.or    r5,r5,r6
        l.lbz   r6,3(r2)
        l.or    r5,r5,r6
        l.srli  r4,r3,19
        l.andi  r4,r4,0x7c
        l.add   r4,r4,r1
        l.j     align_end
        l.sw    0(r4),r5        
                
sh:
        l.srli  r4,r3,9
        l.andi  r4,r4,0x7c
        l.add   r4,r4,r1
        l.lwz   r5,0(r4)        
        l.sb    1(r2),r5
        l.srli  r5,r5,8
        l.j     align_end
        l.sb    0(r2),r5

sw:
        l.srli  r4,r3,9
        l.andi  r4,r4,0x7c
        l.add   r4,r4,r1
        l.lwz   r5,0(r4)        
        l.sb    3(r2),r5
        l.srli  r5,r5,8
        l.sb    2(r2),r5
        l.srli  r5,r5,8
        l.sb    1(r2),r5
        l.srli  r5,r5,8
        l.j     align_end
        l.sb    0(r2),r5

align_end:
	l.j    _ret_from_intr
	l.nop	
#endif	

/* ---[ 0x700: Illegal insn exception ]---------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x700,0x700)

/* ---[ 0x800: External interrupt exception ]---------------------------- */

EXCEPTION_ENTRY(_external_irq_handler)
#ifdef CONFIG_OPENRISC_ESR_EXCEPTION_BUG_CHECK
	l.lwz	r4,SR(r1)		// were interrupts enabled ?
	l.andi	r4,r4,SPR_SR_IEE
	l.sfeqi	r4,0
	l.bnf	1f			// ext irq enabled, all ok.
	l.nop

        l.addi  r1,r1,-0x8
        LOAD_SYMBOL_2_GPR(r3,42f)
        l.sw    0x0(r1),r3
        l.jal   printk
        l.sw    0x4(r1),r4
        l.addi  r1,r1,0x8

	.section .rodata, "a"
42:	
		.string "\n\rESR interrupt bug: in _external_irq_handler (ESR %x)\n\r"
		.align 4
	.previous

	l.ori	r4,r4,SPR_SR_IEE	// fix the bug
//	l.sw	SR(r1),r4
1:	
#endif
	l.addi r3,r1,0
	LOAD_SYMBOL_2_GPR(r8,do_IRQ)
        l.jalr r8
	l.nop
	l.j    _ret_from_intr
	l.nop	
	
/* ---[ 0x900: DTLB miss exception ]------------------------------------- */

		
/* ---[ 0xa00: ITLB miss exception ]------------------------------------- */

		
/* ---[ 0xb00: Range exception ]----------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0xb00,0xb00)

/* ---[ 0xc00: Syscall exception ]--------------------------------------- */

EXCEPTION_ENTRY(_sys_call_handler)
	/* r3 is not tainted by EXCEPTION_HANDLE() */
	
	/* r4 holds the EEAR address of the fault, load the original r4 */
	l.lwz	r4,GPR4(r1)

	l.lwz	r30,TI_FLAGS(r10)
	l.andi	r30,r30,_TIF_SYSCALL_TRACE
	l.sfne	r30,r0
	l.bf	_syscall_trace_entry
	l.nop

	/* check that userspace didn't try to fool us with 
	 * syscall number greater then __NR_syscall. 
	 */	
	l.sfgeui r11,__NR_syscalls
	l.bf	_syscall_badsys
	l.nop
	
	/* WTF: no other arch has this?? */
	ENABLE_INTERRUPTS(r9)		// enable interrupts, r9 is temp

_syscall_call:
        l.movhi r30,hi(sys_call_table)
        l.ori   r30,r30,lo(sys_call_table)
        l.slli  r11,r11,2
        l.add   r30,r30,r11
        l.lwz   r30,0(r30)
	
	l.jalr  r30
	 l.nop

	l.sw    GPR11(r1),r11           // save return value
        l.sw    RESULT(r1),r11          // save result

_syscall_exit:
	DISABLE_INTERRUPTS(r3,r4)
	l.lwz	r30,TI_FLAGS(r10)
	l.andi	r3,r30,_TIF_ALLWORK_MASK
	l.sfne	r3,r0
	l.bf	_syscall_exit_work
	l.nop

	l.j	_restore_all
	l.nop

_syscall_trace_entry:
	l.sw	SYSCALLNO(r1),r11
	l.addi	r3,r1,0
	l.addi	r4,r0,0
	l.jal	do_syscall_trace
	l.nop

	/* Restore arguments */
	l.lwz	r11,SYSCALLNO(r1)
	l.lwz	r3,GPR3(r1)
	l.lwz	r4,GPR4(r1)
	l.lwz	r5,GPR5(r1)
	l.lwz	r6,GPR6(r1)
	l.lwz	r7,GPR7(r1)
	l.sfgeui r11,__NR_syscalls
	l.bnf	_syscall_call
	l.nop

	l.addi	r30,r0,-ENOSYS
	l.sw	GPR11(r1),r30
	l.sw	RESULT(r1),r30

	l.j	_syscall_exit
	l.nop

_syscall_badsys:
	/* Just return -ENOSYS */
	l.addi	r30,r0,-ENOSYS
	l.sw	RESULT(r1),r30
	l.sw	GPR11(r1),r30
	l.j	_resume_userspace
	l.nop

_syscall_exit_work:
	/* r30 holds TI_flags */
	l.andi	r3,r30,_TIF_SYSCALL_TRACE|_TIF_SINGLESTEP
	l.sfne	r3,r0
	l.bnf	_work_pending
	l.nop

	ENABLE_INTERRUPTS(r3)

	l.addi	r3,r1,0
	l.addi	r4,r0,1
	l.jal	do_syscall_trace
	l.nop

	l.j	_resume_userspace
	l.nop


/* ---[ 0xd00: Trap exception ]------------------------------------------ */

UNHANDLED_EXCEPTION(_vector_0xd00,0xd00)

/* ---[ 0xe00: Trap exception ]------------------------------------------ */

EXCEPTION_ENTRY(_trap_handler)
	/* r4: EA of fault (set by EXCEPTION_HANDLE) */
	l.jal   do_trap
	 l.addi  r3,r1,0 /* pt_regs */

	l.j     _ret_from_exception
	 l.nop

/* ---[ 0xf00: Reserved exception ]-------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0xf00,0xf00)

/* ---[ 0x1000: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1000,0x1000)

/* ---[ 0x1100: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1100,0x1100)

/* ---[ 0x1200: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1200,0x1200)

/* ---[ 0x1300: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1300,0x1300)

/* ---[ 0x1400: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1400,0x1400)

/* ---[ 0x1500: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1500,0x1500)

/* ---[ 0x1600: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1600,0x1600)

/* ---[ 0x1700: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1700,0x1700)

/* ---[ 0x1800: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1800,0x1800)

/* ---[ 0x1900: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1900,0x1900)

/* ---[ 0x1a00: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1a00,0x1a00)

/* ---[ 0x1b00: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1b00,0x1b00)

/* ---[ 0x1c00: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1c00,0x1c00)

/* ---[ 0x1d00: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1d00,0x1d00)

/* ---[ 0x1e00: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1e00,0x1e00)

/* ---[ 0x1f00: Reserved exception ]------------------------------------- */

UNHANDLED_EXCEPTION(_vector_0x1f00,0x1f00)

/* ========================================================[ return ] === */

_work_pending:
	/*
	 * if (current_thread_info->flags & _TIF_NEED_RESCHED)
	 *     schedule();
	 */ 
	l.lwz   r5,TI_FLAGS(r10)
	l.andi	r3,r5,_TIF_NEED_RESCHED
	l.sfnei r3,0
	l.bnf   _work_notifysig
	 l.nop
	l.jal   schedule
	 l.nop
	l.j	_resume_userspace
	 l.nop

/* Handle pending signals and notify-resume requests */
_work_notifysig:
	l.ori	r3,r1,0
	/* FIXME: how do we know if this is from syscall or not? */
	l.ori	r4,r0,1
	l.jal	do_notify_resume
	 l.nop

_resume_userspace:
	DISABLE_INTERRUPTS(r3,r4)
	l.lwz	r3,TI_FLAGS(r10)
	l.andi	r3,r3,_TIF_WORK_MASK
	l.sfnei	r3,0
	l.bf	_work_pending
	 l.nop

_restore_all:
	// we need to save KSP here too
	// or else fork's 'eat' stack space
	l.addi  r4,r1,INT_FRAME_SIZE
        l.sw    (TI_KSP)(r10),r4
	RESTORE_ALL
	/* This returns to userspace code */

ENTRY(ret_from_fork)
	l.jal	schedule_tail
	 l.nop
	l.j	_syscall_exit
	 l.nop

ENTRY(_ret_from_intr)
	// i386 version:	
	// GET_CURRENT(%ebx)

ENTRY(_ret_from_exception)
	l.lwz	r4,SR(r1)
        l.andi	r3,r4,SPR_SR_SM
        l.sfeqi	r3,0
        l.bnf	_restore_all
	 l.nop
	l.j	_resume_userspace
	 l.nop

/* ========================================================[ switch ] === */

/*
 * This routine switches between two different tasks.  The process
 * state of one is saved on its kernel stack.  Then the state
 * of the other is restored from its kernel stack.  The memory
 * management hardware is updated to the second process's state.
 * Finally, we can return to the second process, via the 'return'.
 *
 * Note: there are two ways to get to the "going out" portion
 * of this code; either by coming in via the entry (_switch)
 * or via "fork" which must set up an environment equivalent
 * to the "_switch" path.  If you change this (or in particular, the
 * SAVE_REGS macro), you'll have to change the fork code also.
 */


/* _switch MUST never lay on page boundry, cause it runs from
 * effective addresses and beeing interrupted by iTLB miss would kill it. 
 * dTLB miss seams to never accour in the bad place since data accesses
 * are from task structures which are always page aligned.
 *
 * The problem happens in RESTORE_ALL_NO_R11 where we first set the EPCR
 * register, then load the previous register values and only at the end call
 * the l.rfe instruction. If get TLB miss in beetwen the EPCR register gets
 * garbled and we end up calling l.rfe with the wrong EPCR. (same probably
 * holds for ESR)
 *
 * To avoid this problems it is sufficient to align _switch to
 * some nice round number smaller than it's size...
 */
	
	.align 0x400
ENTRY(_switch)
	l.sw    (GPR3-INT_FRAME_SIZE)(r1),r3
        l.mtspr r0,r9,SPR_EPCR_BASE		/* Link register to EPCR */
        l.mfspr r3,r0,SPR_SR			/* From SR to ESR */
        l.mtspr r0,r3,SPR_ESR_BASE
	l.addi	r11,r10,0			/* Save old 'current' to 'last' return value*/
	l.lwz   r3,(GPR3-INT_FRAME_SIZE)(r1)
	PUSH_PT_REGS_NO_TRANS
/* DEBUGING HELP */
	l.or	r21,r1,r0
	l.or	r22,r2,r0
	l.or	r23,r3,r0
	l.or	r24,r4,r0
/* END DEBUGGING HELP */
	l.sw    TI_KSP(r3),r1			/* Save old stack pointer */
        l.lwz   r1,TI_KSP(r4)			/* Load new stack pointer */
	l.addi  r10,r5,0			/* set current (r10) to new */
	RESTORE_ALL_NO_R11
		
/* ==================================================================== */

/* These all use the delay slot for setting the argument register, so the
 * jump is always happening after the l.addi instruction */

ENTRY(sys_clone)
	l.j	_sys_clone
	 l.addi	r7,r1,0
	
ENTRY(sys_fork)
	l.j	_sys_fork
	 l.addi	r3,r1,0

ENTRY(sys_vfork)
	l.j	_sys_vfork
	 l.addi	r3,r1,0
	
ENTRY(sys_execve)
	l.j	_sys_execve
	 l.addi	r6,r1,0

ENTRY(sys_sigaltstack)
	l.j	_sys_sigaltstack
	 l.addi	r5,r1,0

ENTRY(sys_rt_sigreturn)
	l.j	_sys_rt_sigreturn
	 l.addi	r3,r1,0

/* ============================================================[ EOF ]=== */

