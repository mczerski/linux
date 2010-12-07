/*
 * include/asm-or32/processor.h
 *
 * Based on:
 * include/asm-cris/processor.h
 * Copyright (C) 2000, 2001, 2002 Axis Communications AB
 *
 */

#ifndef __ASM_OPENRISC_PROCESSOR_H
#define __ASM_OPENRISC_PROCESSOR_H

#include <asm/spr_defs.h>
#include <asm/page.h>
#include <asm/ptrace.h>
#define STACK_TOP       TASK_SIZE
#define STACK_TOP_MAX	STACK_TOP
/* Kernel and user SR register setting */
#define KERNEL_SR (SPR_SR_DME | SPR_SR_IME | SPR_SR_ICE | SPR_SR_DCE | SPR_SR_SM)
#define USER_SR   (SPR_SR_DME | SPR_SR_IME | SPR_SR_ICE | SPR_SR_DCE | SPR_SR_IEE | SPR_SR_TEE)
/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 */
#define current_text_addr() ({ __label__ _l; _l: &&_l;})

/*
 * User space process size. This is hardcoded into a few places,
 * so don't change it unless you know what you are doing.
 */

#define TASK_SIZE       (0x80000000UL)

/* This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE      (TASK_SIZE / 8 * 3)

#ifndef __ASSEMBLY__

struct task_struct;

typedef struct {
        unsigned long seg;
} mm_segment_t;

struct thread_struct {
#if 0
	unsigned long  usp;     /* user space pointer */
	unsigned long  ksp;     /* kernel stack pointer */
	struct pt_regs *regs;   /* pointer to saved register state */
        mm_segment_t   fs;      /* for get_fs() validation */
	signed long    last_syscall;
#endif
};

/*
 * At user->kernel entry, the pt_regs struct is stacked on the top of the kernel-stack.
 * This macro allows us to find those regs for a task.
 * Notice that subsequent pt_regs stackings, like recursive interrupts occuring while
 * we're in the kernel, won't affect this - only the first user->kernel transition
 * registers are reached by this.
 */
#define user_regs(thread_info) (((struct pt_regs *)((unsigned long)(thread_info) + THREAD_SIZE - STACK_FRAME_OVERHEAD)) - 1)

/*
 * Dito but for the currently running task
 */

#define task_pt_regs(task) user_regs(task_thread_info(task)) 
#define current_regs() task_pt_regs(current) 

extern inline void prepare_to_copy(struct task_struct *tsk)
{
}

#define INIT_SP         (sizeof(init_stack) + (unsigned long) &init_stack)

#define INIT_THREAD  { \
   0, INIT_SP, NULL, KERNEL_DS, 0 }


#define KSTK_EIP(tsk)   (task_pt_regs(tsk)->pc);
#define KSTK_ESP(tsk)   (task_pt_regs(tsk)->sp);


extern int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

void start_thread(struct pt_regs *regs, unsigned long nip, unsigned long sp);
void release_thread(struct task_struct *);
unsigned long get_wchan(struct task_struct *p);

/*
 * Free current thread data structures etc..
 */
 
extern inline void exit_thread(void)
{
         /* Nothing needs to be done.  */
}

/*
 * Return saved PC of a blocked thread. For now, this is the "user" PC
 */
extern unsigned long thread_saved_pc(struct task_struct *t);

#define init_stack      (init_thread_union.stack)

#define cpu_relax()     do { } while (0)

#endif /* __ASSEMBLY__ */
#endif /* __ASM_OPENRISC_PROCESSOR_H */
