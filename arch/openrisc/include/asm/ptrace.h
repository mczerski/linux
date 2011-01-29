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

#ifndef __ASSEMBLY__

struct pt_regs {
	long  pc;
	long  sr;
	long  sp;
	long  gprs[30];
	long  orig_gpr11;  /* Used for restarting system calls */
	long  syscallno;  /* Syscall no. (used by strace) */
};
#endif /* __ASSEMBLY__ */

#ifdef __KERNEL__
#define STACK_FRAME_OVERHEAD  128  /* size of minimum stack frame */
//#define STACK_FRAME_OVERHEAD  0  /* size of minimum stack frame */

#define instruction_pointer(regs) ((regs)->pc)
#define user_mode(regs) (((regs)->sr & SPR_SR_SM) == 0)
#define profile_pc(regs) instruction_pointer(regs)

#endif /* __KERNEL__ */

/*
 * Offsets used by 'ptrace' system call interface.
 */
#define PT_PC        0
#define PT_SR        4
#define PT_SP        8
#define PT_GPR2      12
#define PT_GPR3      16
#define PT_GPR4      20
#define PT_GPR5      24
#define PT_GPR6      28
#define PT_GPR7      32
#define PT_GPR8      36
#define PT_GPR9      40
#define PT_GPR10     44
#define PT_GPR11     48
#define PT_GPR12     52
#define PT_GPR13     56
#define PT_GPR14     60
#define PT_GPR15     64
#define PT_GPR16     68
#define PT_GPR17     72
#define PT_GPR18     76
#define PT_GPR19     80
#define PT_GPR20     84
#define PT_GPR21     88
#define PT_GPR22     92
#define PT_GPR23     96
#define PT_GPR24     100
#define PT_GPR25     104
#define PT_GPR26     108
#define PT_GPR27     112
#define PT_GPR28     116
#define PT_GPR29     120
#define PT_GPR30     124
#define PT_GPR31     128
#define PT_ORIG_GPR11 132
#define PT_SYSCALLNO 140

#endif /* __ASM_OPENRISC_PTRACE_H */
