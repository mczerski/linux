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
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/audit.h>
#include <linux/tracehook.h>
#include <linux/regset.h>

#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/or32-hf.h>

/*
 * retrieve the contents of OpenRISC userspace general registers
 */
static int genregs_get(struct task_struct *target,
		       const struct user_regset *regset,
		       unsigned int pos, unsigned int count,
		       void *kbuf, void __user *ubuf)
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
		       const void *kbuf, const void __user *ubuf)
{
	struct pt_regs *regs = task_pt_regs(target);
	int ret;

	/* PC */
	ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf,
				  regs,
				  0, offsetof(struct pt_regs, sr));

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
		.core_note_type	= NT_PRSTATUS,
		.n		= ELF_NGREG,
		.size		= sizeof(long),
		.align		= sizeof(long),
		.get		= genregs_get,
		.set		= genregs_set,
	},
};

static const struct user_regset_view user_openrisc_native_view = {
	.name		= "OpenRISC",
	.e_machine	= EM_OPENRISC,
	.regsets	= openrisc_regsets,
	.n		= ARRAY_SIZE(openrisc_regsets),
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


#if 0
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
#endif

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
	struct pt_regs* regs;
	unsigned long tmp;

/*	if (off & 3 || off >= sizeof(struct user))
		return -EIO;
*/

	regs = task_pt_regs(tsk);

	tmp = 0;
	if (off == PT_TEXT_ADDR)
		tmp = tsk->mm->start_code;
	else if (off == PT_DATA_ADDR)
		tmp = tsk->mm->start_data;
	else if (off == PT_TEXT_END_ADDR)
		tmp = tsk->mm->end_code;
	else if (off < sizeof(struct pt_regs)) {
		tmp = *((unsigned long*)((char*)regs + off));
	}

	return put_user(tmp, ret);
}

/*
 * Write the word at offset "off" into "struct user".  We
 * actually access the pt_regs stored on the kernel stack.
 */
static int ptrace_write_user(struct task_struct *tsk, unsigned long off,
                             unsigned long val)
{
	struct pt_regs* regs;

/*
	if (off & 3 || off >= sizeof(struct user))
		return -EIO;
*/
	if (off >= sizeof(struct pt_regs))
		return 0;

	regs = task_pt_regs(tsk);

	if (off != offsetof(struct pt_regs, sr)) {
		*((unsigned long*)((char*)regs + off)) = val;
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

	//printk("ptrace request: %d\n", request);

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

	return ret;
}

#if 0
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
#if 0
		if ((addr & 3) || addr < 0 || addr >= sizeof(struct user))
			break;
#endif
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
#if 0
		if ((addr & 3) || addr < 0 || addr >= sizeof(struct user))
			break;
#endif

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
#endif

/* notification of system call entry/exit
 * - triggered by current->work.syscall_trace
 */
asmlinkage long
do_syscall_trace_enter(struct pt_regs *regs)
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

	return ret ?: regs->syscallno;

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
				    regs->regs[6], regs->regs[7]);/*RGD*/
  
#endif
}

asmlinkage void
do_syscall_trace_leave(struct pt_regs* regs)
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
