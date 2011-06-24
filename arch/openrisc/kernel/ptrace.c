/*
 * OpenRISC ptrace.c
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source
 * declaration.
 *
 * Modifications for the OpenRISC architecture:
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2005 Gyorgy Jeney <nog@bsemi.com>
 * Copyright (C) 2010 Julius Baxter <julius.baxter@orsoc.se>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <stddef.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>

#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/audit.h>
#include <linux/regset.h>
#include <linux/tracehook.h>
#include <linux/elf.h>

#include <asm/thread_info.h>
#include <asm/segment.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>

#include "ptrace.h"

/*
 * retrieve the contents of OpenRISC userspace general registers
 */
static int genregs_get(struct task_struct *target,
		       const struct user_regset *regset,
		       unsigned int pos, unsigned int count,
		       void *kbuf, void __user * ubuf)
{
	const struct pt_regs *regs = task_pt_regs(target);
	int ret;

#if 0
	/* r0 */
	ret = user_regset_copyout_zero(&pos, &count, &kbuf, &ubuf,
				       0, offsetof(struct pt_regs, regs));
#endif

	ret = user_regset_copyout(&pos, &count, &kbuf, &ubuf,
				  regs, 0, sizeof(*regs));

/* put PPC here */

#if 0
	/* fill out rest of elf_gregset_t structure with zeroes */
	if (!ret)
		ret = user_regset_copyout_zero(&pos, &count, &kbuf, &ubuf,
					       sizeof(struct pt_regs), -1);
#endif

	return ret;
}

/*
 * update the contents of the OpenRISC userspace general registers
 */
static int genregs_set(struct task_struct *target,
		       const struct user_regset *regset,
		       unsigned int pos, unsigned int count,
		       const void *kbuf, const void __user * ubuf)
{
	struct pt_regs *regs = task_pt_regs(target);
	int ret;

	/* PC */
	ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf,
				 regs, 0, offsetof(struct pt_regs, sr));

	/* skip SR */
	ret = user_regset_copyin_ignore(&pos, &count, &kbuf, &ubuf,
					offsetof(struct pt_regs, sr),
					offsetof(struct pt_regs, sp));

	/* SP, r2 - r31 */
	ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf,
				 regs,
				 offsetof(struct pt_regs, sp),
				 sizeof(struct pt_regs));

#if 0
	/* read out the rest of the elf_gregset_t structure */
	if (!ret)
		ret = user_regset_copyin_ignore(&pos, &count, &kbuf, &ubuf,
						sizeof(struct pt_regs), -1);
#endif

	return ret;
}

/*
 * Define the register sets available on OpenRISC under Linux
 */
enum openrisc_regset {
	REGSET_GENERAL,
};

static const struct user_regset openrisc_regsets[] = {
	[REGSET_GENERAL] = {
			    .core_note_type = NT_PRSTATUS,
			    .n = ELF_NGREG,
			    .size = sizeof(long),
			    .align = sizeof(long),
			    .get = genregs_get,
			    .set = genregs_set,
			    },
};

static const struct user_regset_view user_openrisc_native_view = {
	.name = "OpenRISC",
	.e_machine = EM_OPENRISC,
	.regsets = openrisc_regsets,
	.n = ARRAY_SIZE(openrisc_regsets),
};

const struct user_regset_view *task_user_regset_view(struct task_struct *task)
{
	return &user_openrisc_native_view;
}

void user_enable_single_step(struct task_struct *child)
{
	set_tsk_thread_flag(child, TIF_SINGLESTEP);
}

void user_disable_single_step(struct task_struct *child)
{
	clear_tsk_thread_flag(child, TIF_SINGLESTEP);
}

/*
 * does not yet catch signals sent when the child dies.
 * in exit.c or in signal.c.
 */

#define OPC_MASK 0x3f
#define OPC_SHIFT 26
#define OPC_J    0x00
#define OPC_JAL  0x01
#define OPC_BNF  0x03
#define OPC_BF   0x04
#define OPC_SYSC 0x08
#define OPC_JR   0x11
#define OPC_JALR 0x12

#define JUMP_IMM_MASK 0x03ffffff
#define JUMP_IMM_SIGN 0x02000000

/* Trap instruction on bit 15 of SR, fixed one, unconditional */
#define OR1K_TRAP 0x2100000f

/* Macro to extract branch offset with sign extension of immediate. */
#define BRANCH_OFFSET(insn) ((insn & JUMP_IMM_SIGN) ?			\
			     ((insn & JUMP_IMM_MASK) << 2) | 0xf0000000 : \
			     ((insn & JUMP_IMM_MASK) << 2))

/* Get address of next instruction we can insert a breakpoint on */
/* pc passed to function is address of instruction we've now just trapped on.
   It is yet/next to be executed. We determine next instruction after PC to
   place a l.trap on. If we should take a branch, we must remember this.
*/
static unsigned long
get_next_address(struct task_struct *tsk, unsigned long pc, unsigned long insn)
{
	struct pt_regs *regs;
	struct debug_info *dbg;
	char opc;
	unsigned long npc;
	unsigned long rB;

	dbg = &current_thread_info()->debug;

	regs = task_pt_regs(tsk);
	/* Extract opcode from instruction */
	opc = (insn >> OPC_SHIFT) & OPC_MASK;

	pr_debug(KERN_INFO
		 "ptrace get_next_address: pc 0x%.8lx insn. 0x%.8lx opc. 0x%.2x\n",
		 pc, insn, opc);

	/*
	   Will always proceed to next instruction if this function was called
	   ie. we know we're not in a delay slot - the function calling this
	   checks dbg->branch_taken.
	 */
	npc = pc + 4;

	/* Check if we're to record a branch */
	switch (opc) {
	case OPC_BNF:
		/* Check flag - we're branching if !flag */
		if (!(((regs)->sr) & SPR_SR_F)) {
			dbg->bp.branch = 1;
			dbg->bp.branch_target = pc + BRANCH_OFFSET(insn);
		}
		break;
	case OPC_BF:
		/* Check flag - we're branching if flag */
		if (((regs)->sr) & SPR_SR_F) {
			dbg->bp.branch = 1;
			dbg->bp.branch_target = pc + BRANCH_OFFSET(insn);
		}
		break;
	case OPC_J:
	case OPC_JAL:
		/* PC-relative branch target encoded in instruction. Extract
		   and add it. */
		dbg->bp.branch = 1;
		dbg->bp.branch_target = pc + BRANCH_OFFSET(insn);
		break;
	case OPC_JR:
	case OPC_JALR:
		/* Register number holding branch target is encoded in rB slot
		   of instruction. Extract it. */
		rB = (insn >> 11) & 0x1f;
		pr_debug(KERN_INFO
			 "ptrace get_next_address: jump reg from r%ld=0x%08lx\n",
			 rB,
			 /* *((unsigned long*)((char*)regs + (rB<<2) + 4)) */
			 regs->gprs[rB - 2]);
		if (rB < 2)
			pr_debug(KERN_WARNING
				 "ptrace get_next_address(): Warning, JR with GPR < 2");
		dbg->bp.branch = 1;
		dbg->bp.branch_target =
		    /* *((unsigned long*)((char*)regs + (rB<<2) + 4)) */
		    regs->gprs[rB - 2];
		break;
#if 0
	case OPC_SYSC:
		/* Not sure we want to do this */
		/* Remember - l.sys has no delay slot. */
		npc = 0x900;
		break;
#endif
	default:
		break;
	}

	/* If setting a branch target, remember where branch was */
	if (dbg->bp.branch)
		dbg->bp.branch_insn_address = pc;

	if (dbg->bp.branch)
		pr_debug(KERN_INFO
			 "ptrace get_next_address: branch detected to 0x%.8lx\n",
			 dbg->bp.branch_target);
	pr_debug(KERN_INFO "ptrace get_next_address: returning 0x%.8lx\n", npc);

	return npc;
}

static inline int
read_instr(struct task_struct *tsk, unsigned long addr, u32 * res)
{
	int ret;
	u32 val;
	ret = access_process_vm(tsk, addr & ~3, &val, sizeof(val), 0);
	ret = ret == sizeof(val) ? 0 : -EIO;
	*res = val;
	return ret;
}

static int
swap_insn(struct task_struct *tsk, unsigned long addr,
	  void *old_insn, void *new_insn, int size)
{
	int ret;

	ret = access_process_vm(tsk, addr, old_insn, size, 0);
	if (ret == size)
		ret = access_process_vm(tsk, addr, new_insn, size, 1);
	return ret;
}

static void
add_breakpoint(struct task_struct *tsk, struct debug_info *dbg,
	       unsigned long addr)
{
	u32 new_insn = OR1K_TRAP;
	int res;

	res = swap_insn(tsk, addr, &dbg->bp.insn, &new_insn, 4);

	pr_debug(KERN_INFO
		 "ptrace add_breakpoint: addr 0x%.8lx insn 0x%.8lx %d\n\n",
		 addr, dbg->bp.insn, res);

	if (res == 4) {
		dbg->bp.address = addr;
		dbg->bp.set = 1;
	}
}

/*
 * Clear the breakpoint in the user program.
 */
static void clear_breakpoint(struct task_struct *tsk, struct debug_entry *bp)
{
	u32 old_insn;
	int ret;
	struct pt_regs *regs;
	unsigned long pc;

	unsigned long addr = bp->address;

	regs = task_pt_regs(tsk);
	pc = instruction_pointer(regs);	/* This is NPC */

	/* Either at the breakpoint, or in delay slot (pc will be at address of
	 * branch instruction - OR1K exception handlers do that, for now.
	 * Perhaps FIXME
	 */
	if ((pc == addr) || ((bp->branch_insn_address == pc) && bp->branch))
		ret = swap_insn(tsk, addr & ~3, &old_insn, &bp->insn, 4);
	else {
		/* Trapped for some other reason. */
		pr_debug(KERN_INFO
			 "ptrace clear_breakpoint: not correct, pc 0x%.8lx bp 0x%.8lx\n",
			 pc, addr);
		pr_debug(KERN_INFO
			 "ptrace clear_breakpoint: 0x%.8lx 0x%.8lx %ld\n",
			 bp->branch_insn_address, bp->branch_target,
			 bp->branch);
		return;
	}

	pr_debug(KERN_INFO
		 "ptrace clear_breakpoint: addr 0x%.8lx insn 0x%.8lx %d\n",
		 addr, bp->insn, ret);

	if (ret != 4 || old_insn != OR1K_TRAP)
		pr_debug(KERN_ERR
			 "%s:%d: corrupted breakpoint at 0x%08lx (0x%08x)\n",
			 tsk->comm, task_pid_nr(tsk), addr, old_insn);

	bp->set = 0;
}

void ptrace_set_bpt(struct task_struct *tsk)
{
	struct pt_regs *regs;
	unsigned long pc;
	u32 insn;
	int res;

	regs = task_pt_regs(tsk);
	pc = instruction_pointer(regs);	/* This is NPC */

	res = read_instr(tsk, pc, &insn);
	if (!res) {
		struct debug_info *dbg = &current_thread_info()->debug;
		unsigned long npc;

		/* First check if breakpoint is still set, if so, probably
		   just let us run until we hit it.
		   This can occur because perhaps we caused an exception before
		   a jump or in a delay slot, and the OR1K exception handlers
		   will rewind us to the branch before the delay slot or jump
		   target.
		   This way we skip everything that goes on during an exception
		   and can hopefully just track the program's execution.
		 */
		if (dbg->bp.set)
			return;

		/* Are we in delay slot of branch we should take? */
		if (dbg->bp.branch == 1) {
			/* Branch target is already determined. Set l.trap. */
			npc = dbg->bp.branch_target;

			/* Reset NPC to branch instruction - should be one
			 * before this one.
			 */
			instruction_pointer(regs) = dbg->bp.branch_insn_address;

			/* Now mark this as no longer needing to be taken */
			dbg->bp.branch = 0;
		} else {
			npc = get_next_address(tsk, pc, insn);
		}

		add_breakpoint(tsk, dbg, npc);
	}
}

/*
 * Ensure no single-step breakpoint is pending.  Returns non-zero
 * value if child was being single-stepped.
 */
void ptrace_cancel_bpt(struct task_struct *tsk)
{
	clear_breakpoint(tsk,
			 (struct debug_entry *)&current_thread_info()->debug);
}

/*
 * Called by kernel/ptrace.c when detaching..
 *
 * Make sure the single step bit is not set.
 */
void ptrace_disable(struct task_struct *child)
{
	pr_debug(KERN_WARNING "ptrace_disable(): TODO\n");

	user_disable_single_step(child);
	clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
}

/*
 * Read the word at offset "off" into the "struct user".  We
 * actually access the pt_regs stored on the kernel stack.
 */
static int ptrace_read_user(struct task_struct *tsk, unsigned long off,
			    unsigned long __user *ret)
{
	struct pt_regs *regs;
	unsigned long tmp;

	/*    if (off & 3 || off >= sizeof(struct user))
	   return -EIO; */

	regs = task_pt_regs(tsk);

	tmp = 0;
	if (off == PT_TEXT_ADDR)
		tmp = tsk->mm->start_code;
	else if (off == PT_DATA_ADDR)
		tmp = tsk->mm->start_data;
	else if (off == PT_TEXT_END_ADDR)
		tmp = tsk->mm->end_code;
	else if (off < sizeof(struct pt_regs))
		tmp = *((unsigned long *)((char *)regs + off));

	return put_user(tmp, ret);
}

/*
 * Write the word at offset "off" into "struct user".  We
 * actually access the pt_regs stored on the kernel stack.
 */
static int ptrace_write_user(struct task_struct *tsk, unsigned long off,
			     unsigned long val)
{
	struct pt_regs *regs;

	/*if (off & 3 || off >= sizeof(struct user))
	   return -EIO; */

	if (off >= sizeof(struct pt_regs))
		return 0;

	regs = task_pt_regs(tsk);

	if (off != offsetof(struct pt_regs, sr)) {
		*((unsigned long *)((char *)regs + off)) = val;
	} else {
		/* Prevent any process from setting the SR flags and
		 * thus elevating privileges
		 */
	}

	return 0;
}

long arch_ptrace(struct task_struct *child, long request, unsigned long addr,
		 unsigned long data)
{
	int ret;
	unsigned long __user *datap = (unsigned long __user *)data;

	switch (request) {
		/* read the word at location addr in the USER area. */
	case PTRACE_PEEKUSR:
		ret = ptrace_read_user(child, addr, datap);
		break;
	case PTRACE_POKEUSR:
		ret = ptrace_write_user(child, addr, data);
		break;
	default:
		ret = ptrace_request(child, request, addr, data);
		break;
	}

	single_step_set(current);

	return ret;
}

/* notification of system call entry/exit
 * - triggered by current->work.syscall_trace
 */
asmlinkage long do_syscall_trace_enter(struct pt_regs *regs)
{
	long ret = 0;

	if (test_thread_flag(TIF_SYSCALL_TRACE) &&
	    tracehook_report_syscall_entry(regs))
		/*
		 * Tracing decided this syscall should not happen.
		 * We'll return a bogus call number to get an ENOSYS
		 * error, but leave the original number in <something>.
		 */
		ret = -1L;

/*	if (unlikely(test_thread_flag(TIF_SYSCALL_TRACEPOINT)))
		trace_sys_enter(regs, regs->syscallno);
*/

	/* Are these regs right??? */
	if (unlikely(current->audit_context))
		audit_syscall_entry(audit_arch(), regs->syscallno,
				    regs->gpr[3], regs->gpr[4],
				    regs->gpr[5], regs->gpr[6]);

	return ret ? : regs->syscallno;

#if 0
/*FIXME : audit the rest of this */

	/*
	 * this isn't the same as continuing with a signal, but it will do
	 * for normal use.  strace only continues with a signal if the
	 * stopping signal is not SIGTRAP.  -brl
	 */
	if (current->exit_code) {
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}
out:
	/*FIXME: audit_arch isn't even defined for openrisc */
	/*FIXME:  What's with the register numbers here... makes no sense */
	if (unlikely(current->audit_context) && !entryexit)
		audit_syscall_entry(audit_arch(), regs->regs[2],
				    regs->regs[4], regs->regs[5],
				    regs->regs[6], regs->regs[7]);
	/*RGD*/
#endif
}

asmlinkage void do_syscall_trace_leave(struct pt_regs *regs)
{
	int step;

	if (unlikely(current->audit_context))
		audit_syscall_exit(AUDITSC_RESULT(regs->gpr[11]),
				   regs->gpr[11]);

/*	if (unlikely(test_thread_flag(TIF_SYSCALL_TRACEPOINT)))
		trace_sys_exit(regs, regs->gprs[9]);
*/

	step = test_thread_flag(TIF_SINGLESTEP);
	if (step || test_thread_flag(TIF_SYSCALL_TRACE))
		tracehook_report_syscall_exit(regs, step);
}
