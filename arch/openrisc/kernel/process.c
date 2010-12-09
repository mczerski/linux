/*
 *  linux/arch/or32/kernel/process.c
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
 *
 * This file handles the architecture-dependent parts of process handling..
 * Based on m86k.
 */

#define __KERNEL_SYSCALLS__
#include <stdarg.h>

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/elfcore.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/init_task.h>
#include <linux/mqueue.h>
#include <linux/fs.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/spr_defs.h>
#include <asm/or32-hf.h>


#include <linux/smp.h>

/*
 * Pointer to Current thread info structure.
 *
 * Used at user space -> kernel transitions. 
 */
struct thread_info *current_thread_info_set[NR_CPUS] = {&init_thread_info, };

/*
 * The hlt_counter, disable_hlt and enable_hlt is just here as a hook if
 * there would ever be a halt sequence (for power save when idle) with
 * some largish delay when halting or resuming *and* a driver that can't
 * afford that delay.  The hlt_counter would then be checked before
 * executing the halt sequence, and the driver marks the unhaltable
 * region by enable_hlt/disable_hlt.
 */

static int hlt_counter=0;

void disable_hlt(void)
{
	hlt_counter++;
}

EXPORT_SYMBOL(disable_hlt);

void enable_hlt(void)
{
	hlt_counter--;
}

EXPORT_SYMBOL(enable_hlt);
 
void machine_restart(void)
{
	printk("*** MACHINE RESTART ***\n");
	__asm__("l.nop 1");
}

EXPORT_SYMBOL(machine_restart);

/*
 * Similar to machine_power_off, but don't shut off power.  Add code
 * here to freeze the system for e.g. post-mortem debug purpose when
 * possible.  This halt has nothing to do with the idle halt.
 */

void machine_halt(void)
{
	printk("*** MACHINE HALT ***\n");
	__asm__("l.nop 1");
}

EXPORT_SYMBOL(machine_halt);

/* If or when software power-off is implemented, add code here.  */

void machine_power_off(void)
{
	printk("*** MACHINE POWER OFF ***\n");
	__asm__("l.nop 1");
}

EXPORT_SYMBOL(machine_power_off);

void (*pm_power_off)(void) = machine_power_off;
EXPORT_SYMBOL(pm_power_off);

/*
 * When a process does an "exec", machine state like FPU and debug
 * registers need to be reset.  This is a hook function for that.
 * Currently we don't have any such state to reset, so this is empty.
 */

void flush_thread(void)
{
}

void show_regs(struct pt_regs *regs)
{
	extern void show_registers(struct pt_regs *regs);

	/* __PHX__ cleanup this mess */
	show_registers(regs);
}

unsigned long thread_saved_pc(struct task_struct *t)
{
	return (unsigned long)user_regs(t->stack)->pc;
}

void release_thread(struct task_struct *dead_task)
{
}

/*
 * Copy the thread-specific (arch specific) info from the current
 * process to the new one p
 */

extern asmlinkage void ret_from_fork(void);

int 
copy_thread(unsigned long clone_flags, unsigned long usp,
	    unsigned long unused,
	    struct task_struct *p, struct pt_regs *regs)
{
	struct pt_regs* childregs;
	struct pt_regs* kregs;
	unsigned long sp = (unsigned long)task_stack_page(p) + THREAD_SIZE;
	struct thread_info *ti;

	p->set_child_tid = p->clear_child_tid = NULL;

	/* Copy registers */
	/* redzone */
	sp -= STACK_FRAME_OVERHEAD;
	sp -= sizeof(struct pt_regs);
	childregs = (struct pt_regs *) sp;

	/* Copy parent registers */
	*childregs = *regs;
        
	if ((childregs->sr & SPR_SR_SM) == 1) {
                /* for kernel thread, set `current_thread_info'
	         * and stackptr in new task
		 */
		childregs->sp = (unsigned long)task_stack_page(p) + THREAD_SIZE;
                childregs->gprs[8] = (unsigned long)task_thread_info(p);
        } else {
		childregs->sp = usp;
	}

        childregs->gprs[9] = 0;  /* Result from fork() */

        /*
         * The way this works is that at some point in the future
         * some task will call _switch to switch to the new task.
         * That will pop off the stack frame created below and start
         * the new task running at ret_from_fork.  The new task will
         * do some house keeping and then return from the fork or clone
         * system call, using the stack frame created above.
         */
	/* redzone */
     	sp -= STACK_FRAME_OVERHEAD;
	sp -= sizeof(struct pt_regs);
        kregs = (struct pt_regs *) sp;
 
	ti = task_thread_info(p);
        ti->ksp = sp;
	
	kregs->sr = regs->sr | SPR_SR_SM;
	kregs->sp = sp + sizeof(struct pt_regs) + STACK_FRAME_OVERHEAD;
	kregs->gprs[1] = (unsigned long)current;  /* arg to schedule_tail */
	kregs->gprs[8] = (unsigned long)task_thread_info(p);
        kregs->pc = (unsigned long)ret_from_fork;

        return 0;
}

/*
 * Set up a thread for executing a new program
 */
void start_thread(struct pt_regs *regs, unsigned long pc, unsigned long sp)
{
	phx_warn("NIP: %lx, SP: %lx", pc, sp);

	set_fs(USER_DS);
	memset(regs->gprs, 0, sizeof(regs->gprs));

	regs->pc = pc;
	regs->sr = regs->sr & ~ SPR_SR_SM;
	regs->sp = sp;
}

/* Fill in the fpu structure for a core dump.  */
int dump_fpu(struct pt_regs *regs, elf_fpregset_t *fpu)
{
	phx_warn("FPU :: TODO");
	return 0;
} 

void _switch_to(struct task_struct *old, 
		struct task_struct *new,
		struct task_struct **last)
{
	extern struct thread_info *_switch(struct thread_info *old_ti,
					   struct thread_info *new_ti);
	struct thread_info *new_ti, *old_ti;
	unsigned long flags;

	local_irq_save(flags);
#if 0
	check_stack(NULL, __FILE__, __FUNCTION__, __LINE__);
#endif
	/* current_set is an array of saved current pointers
	 * (one for each cpu). we need them at user->kernel transition,
	 * while we save them at kernel->user transition
	 */
	new_ti = new->stack;
	old_ti = old->stack;

	current_thread_info_set[smp_processor_id()] = new_ti;
	*last = (_switch(old_ti, new_ti))->task;
#if 0
	check_stack(NULL, __FILE__, __FUNCTION__, __LINE__);
#endif
	local_irq_restore(flags);
} 

/*
 * fill in the user structure for a core dump..
 */
void dump_thread(struct pt_regs *regs, struct user *dump)
{
	phx_warn("TODO");
}

static void __noreturn kernel_thread_helper(int (*fn)(void *), void *arg)
{
        do_exit(fn(arg));
}

/*
 * Create a kernel thread.
 */
int kernel_thread(int (*fn)(void *), void *arg, unsigned long flags)
{
        struct pt_regs regs;

        memset(&regs, 0, sizeof(regs));

        regs.gprs[1] = (unsigned long)fn;
        regs.gprs[2] = (unsigned long)arg;
        regs.sr = mfspr(SPR_SR);
        regs.pc = (unsigned long)kernel_thread_helper;

        return do_fork(flags | CLONE_VM | CLONE_UNTRACED,
                        0, &regs, 0, NULL, NULL);
}

/*
 * sys_execve() executes a new program.
 */
asmlinkage long _sys_execve(const char __user *name,
                           const char __user *const __user *argv,
                           const char __user *const __user *envp,
                           struct pt_regs *regs)
{
	int error;
	char * filename;

	filename = getname(name);
	error = PTR_ERR(filename);

	if (IS_ERR(filename))
	  goto out;

	error = do_execve(filename, argv, envp, regs);
	putname(filename);
 
out:	
	return error;
}

unsigned long get_wchan(struct task_struct *p)
{
	phx_warn("TODO");

	return 0;
}

int kernel_execve(const char *filename, char *const argv[], char *const 
envp[])
{
  register long __res asm("r11") = __NR_execve;
  register long __a asm("r3") = (long)(filename);
  register long __b asm("r4") = (long)(argv);
  register long __c asm("r5") = (long)(envp);
  __asm__ volatile ("l.sys 1" : "=r" (__res)
           : "r" (__res), "r" (__a), "r" (__b), "r" (__c));
  __asm__ volatile("l.nop");
  return __res;
}
