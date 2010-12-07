/*
 *  linux/arch/or32/kernel/ptrace.c
 *
 *  or32 version
 *    author(s): Matjaz Breskvar (phoenix@bsemi.com)
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
 *  31. 12. 2005: Gyorgy Jeney (nog@bsemi.com)
 *    Added actual ptrace implementation except for single step traceing
 *    (Basically copy arch/i386/kernel/ptrace.c)
 *
 *  Based on:
 *
 *  linux/arch/m68k/kernel/ptrace
 *
 *  Copyright (C) 1994 by Hamish Macdonald
 *  Taken from linux/kernel/ptrace.c and modified for M680x0.
 *  linux/kernel/ptrace.c is by Ross Biro 1/23/92, edited by Linus Torvalds
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of
 * this archive for more details.
 */

#include <stddef.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>

#include <linux/mm.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/audit.h>

#include <asm/segment.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/or32-hf.h>

/*
 * does not yet catch signals sent when the child dies.
 * in exit.c or in signal.c.
 */


/*
 * Get contents of register REGNO in task TASK.
 */
static inline long get_reg(struct task_struct *task, int regno)
{
	if(regno < sizeof(struct pt_regs))
		return *((unsigned long *)user_regs(task->stack) + (regno >> 2));
	switch(regno) {
	case offsetof(struct user, start_code):
		return task->mm->start_code;
	case offsetof(struct user, start_data):
		return task->mm->start_data;
	case offsetof(struct user, start_stack):
		return task->mm->start_stack;
	default:
		printk("Don't know how to get offset %i of struct user\n",
		       regno);
	}
	return 0;
}

/*
 * Write contents of register REGNO in task TASK.
 */
static inline int put_reg(struct task_struct *task, int regno,
			  unsigned long data)
{
	/* Whatever happens, don't let any process the the sr flags and elevate
	 * their privaleges to kernel mode. */
	if((regno < sizeof(struct pt_regs)) &&
	   (regno != offsetof(struct pt_regs, sr))) {
		*((unsigned long *)task_pt_regs(task) + (regno >> 2)) = data;
		return 0;
	}
	return -EIO;
}

static void set_singlestep(struct task_struct *child)
{
	/* FIXME */
#if 0
	struct pt_regs *regs = get_child_regs(child);

	/*
	 * Always set TIF_SINGLESTEP - this guarantees that 
	 * we single-step system calls etc..  This will also
	 * cause us to set TF when returning to user mode.
	 */
	set_tsk_thread_flag(child, TIF_SINGLESTEP);

	/*
	 * If TF was already set, don't do anything else
	 */
	if (regs->eflags & TRAP_FLAG)
		return;

	/* Set TF on the kernel stack.. */
	regs->eflags |= TRAP_FLAG;

	child->ptrace |= PT_DTRACE;
#endif
}


static void clear_singlestep(struct task_struct *child)
{
	/* FIXME */
#if 0
	/* Always clear TIF_SINGLESTEP... */
	clear_tsk_thread_flag(child, TIF_SINGLESTEP);

	/* But touch TF only if it was set by us.. */
	if (child->ptrace & PT_DTRACE) {
		struct pt_regs *regs = get_child_regs(child);
		regs->eflags &= ~TRAP_FLAG;
		child->ptrace &= ~PT_DTRACE;
	}
#endif
}

/*
 * Called by kernel/ptrace.c when detaching..
 *
 * Make sure the single step bit is not set.
 */
void ptrace_disable(struct task_struct *child)
{
	printk("ptrace_disable(): TODO\n");
	clear_singlestep(child);
	clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
}

long arch_ptrace(struct task_struct *child, long request, unsigned long addr,
	         unsigned long data)
{
	int ret;
	unsigned long __user *datap = (unsigned long __user *)data;

	switch (request) {
	/* when I and D space are separate, these will need to be fixed. */
	case PTRACE_PEEKTEXT: /* read word at location addr. */ 
	case PTRACE_PEEKDATA: {
		unsigned long tmp;
		int copied;

		copied = access_process_vm(child, addr, &tmp, sizeof(tmp), 0);
		ret = -EIO;
		if (copied != sizeof(tmp))
			break;
		ret = put_user(tmp,(unsigned long *) datap);
		break;
	}

	/* read the word at location addr in the USER area. */
	case PTRACE_PEEKUSR:
		ret = -EIO;
		if ((addr & 3) || addr < 0 || addr >= sizeof(struct user))
			break;

		ret = put_user(get_reg(child, addr), datap);
		break;

	/* when I and D space are separate, this will have to be fixed. */
	case PTRACE_POKETEXT: /* write the word at location addr. */
	case PTRACE_POKEDATA:
		ret = 0;
		if (access_process_vm(child, addr, &data, sizeof(data), 1) == sizeof(data))
			break;
		ret = -EIO;
		break;

	case PTRACE_POKEUSR: /* write the word at location addr in the USER area */
		ret = -EIO;
		if ((addr & 3) || addr < 0 || addr >= sizeof(struct user))
			break;

		ret = put_reg(child, addr, data);	    
		break;

	case PTRACE_SYSCALL: /* continue and stop at next (return from) syscall */
	case PTRACE_CONT: /* restart after signal. */
		ret = -EIO;
		if (!valid_signal(data))
			break;
		if (request == PTRACE_SYSCALL) {
			set_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
		} else {
			clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
		}
		child->exit_code = data;
		/* make sure the single step bit is not set. */
		clear_singlestep(child);
		wake_up_process(child);
		ret = 0;
		break;

/*
 * make the child exit.  Best I can do is send it a sigkill. 
 * perhaps it should be put in the status that it wants to 
 * exit.
 */
	case PTRACE_KILL:
		ret = 0;
		if (child->exit_state == EXIT_ZOMBIE)	/* already dead */
			break;
		child->exit_code = SIGKILL;
		/* make sure the single step bit is not set. */
		clear_singlestep(child);
		wake_up_process(child);
		break;

		/*case PTRACE_SYSEMU_SINGLESTEP:  Same as SYSEMU, but singlestep if not syscall RGD */
	case PTRACE_SINGLESTEP:  /* set the trap flag. */
		printk("SINGLESTEP: TODO\n");
		ret = -ENOSYS;
#if 0
		ret = -EIO;
		if (!valid_signal(data))
			break;

		if (request == PTRACE_SYSEMU_SINGLESTEP)
			set_tsk_thread_flag(child, TIF_SYSCALL_EMU);
		else
			clear_tsk_thread_flag(child, TIF_SYSCALL_EMU);

		clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
		set_singlestep(child);
		child->exit_code = data;
		/* give it a chance to run. */
		wake_up_process(child);
		ret = 0;
#endif
		break;

	case PTRACE_DETACH: /* detach a process that was attached. */
		ret = ptrace_detach(child, data);
		break;

	default:
		ret = ptrace_request(child, request, addr, data);
		break;
	}
	return ret;
}

/* notification of system call entry/exit
 * - triggered by current->work.syscall_trace
 */
asmlinkage void do_syscall_trace(struct pt_regs *regs, int entryexit)
{
 	if (unlikely(current->audit_context) && entryexit)
	  audit_syscall_exit(AUDITSC_RESULT(regs->regs[2]), regs->regs[2]);/*RGD current*/

	if (!test_thread_flag(TIF_SYSCALL_TRACE))
		goto out;
	if (!(current->ptrace & PT_PTRACED))
		goto out;

	/* The 0x80 provides a way for the tracing parent to distinguish
	   between a syscall stop and SIGTRAP delivery */
	ptrace_notify(SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD) ?
	                         0x80 : 0));

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
	if (unlikely(current->audit_context) && !entryexit)
		audit_syscall_entry(audit_arch(), regs->regs[2],
				    regs->regs[4], regs->regs[5],
				    regs->regs[6], regs->regs[7]);/*RGD*/
  
}
