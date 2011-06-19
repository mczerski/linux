/*
 * OpenRISC Linux
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source
 * declaration.
 *
 * OpenRISC implementation:
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 * et al.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __ASM_OPENRISC_PTRACE_H
#define __ASM_OPENRISC_PTRACE_H

#include <asm/spr_defs.h>
/*
 * This struct defines the way the registers are stored on the
 * kernel stack during a system call or other kernel entry.
 *
 * this should only contain volatile regs
 * since we can keep non-volatile in the thread_struct
 * should set this up when only volatiles are saved
 * by intr code.
 *
 * Since this is going on the stack, *CARE MUST BE TAKEN* to insure
 * that the overall structure is a multiple of 16 bytes in length.
 *
 * Note that the offsets of the fields in this struct correspond with
 * the values below.
 */

/*
 * These are 'magic' values for PTRACE_PEEKUSR that return info about where a
 * process is located in memory.
 */
#define PT_TEXT_ADDR            0x10000
#define PT_DATA_ADDR            0x10004
#define PT_TEXT_END_ADDR        0x10008

#ifndef __ASSEMBLY__

struct pt_regs {
	union {
		struct {
			/* Named registers */
			long  sr;	/* Stored in place of r0 */
			long  sp;	/* r1 */
		};
		struct {
			/* Old style */
			long offset[2];
			long gprs[30];
		};
		struct {
			/* New style */
			long gpr[32];
		};
	};
	long  pc;
	long  orig_gpr11;  /* Used for restarting system calls */
	long  syscallno;  /* Syscall no. (used by strace) */
};
#endif /* __ASSEMBLY__ */

#ifdef __KERNEL__
#define STACK_FRAME_OVERHEAD  128  /* size of minimum stack frame */
//#define STACK_FRAME_OVERHEAD  0  /* size of minimum stack frame */

#define instruction_pointer(regs)	((regs)->pc)
#define user_mode(regs)			(((regs)->sr & SPR_SR_SM) == 0)
#define user_stack_pointer(regs)	((unsigned long)(regs)->sp)
#define profile_pc(regs)		instruction_pointer(regs)

#define arch_has_single_step()  (1)

#endif /* __KERNEL__ */

/*
 * Offsets used by 'ptrace' system call interface.
 */
#define PT_SR         0
#define PT_SP         4
#define PT_GPR2       8
#define PT_GPR3       12
#define PT_GPR4       16
#define PT_GPR5       20
#define PT_GPR6       24
#define PT_GPR7       28
#define PT_GPR8       32
#define PT_GPR9       36
#define PT_GPR10      40
#define PT_GPR11      44
#define PT_GPR12      48
#define PT_GPR13      52
#define PT_GPR14      56
#define PT_GPR15      60
#define PT_GPR16      64
#define PT_GPR17      68
#define PT_GPR18      72
#define PT_GPR19      76
#define PT_GPR20      80
#define PT_GPR21      84
#define PT_GPR22      88
#define PT_GPR23      92
#define PT_GPR24      96
#define PT_GPR25      100
#define PT_GPR26      104
#define PT_GPR27      108
#define PT_GPR28      112
#define PT_GPR29      116
#define PT_GPR30      120
#define PT_GPR31      124
#define PT_PC	      128
#define PT_ORIG_GPR11 132
#define PT_SYSCALLNO  136

#endif /* __ASM_OPENRISC_PTRACE_H */
