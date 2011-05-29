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
 *
 * OpenRISC helper functions
 */

#ifndef __ASM_OPENRISC_HF_H
#define __ASM_OPENRISC_HF_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>

extern int __phx_signal__;
#define phx_signal(fmt, args...)                                \
        if (__phx_signal__)                                     \
            printk("__SIGN__ :: %s: %d: %s() :: " fmt "\n",     \
                           __FILE__, __LINE__, __FUNCTION__, ##args);

extern int __phx_warn__;
#define phx_warn(fmt, args...)                                  \
        if (__phx_warn__)                                       \
            printk("__WARN__ :: %s: %d: %s() :: " fmt "\n",     \
                           __FILE__, __LINE__, __FUNCTION__, ##args);

extern int __phx_mmu__;
#define phx_mmu(fmt, args...)                                   \
        if (__phx_mmu__)                                        \
            printk("__xMMU__ :: %s: %d: %s() :: " fmt "\n",     \
                           __FILE__, __LINE__, __FUNCTION__, ##args);

#define __PHX_DEBUG__
#ifdef __PHX_DEBUG__
extern int __phx_debug__;
# define phx_printk(fmt, args...)                               \
        if (__phx_debug__)                                      \
            printk("__PHX__ :: %s: %d: %s() :: " fmt "\n",      \
                           __FILE__, __LINE__, __FUNCTION__, ##args);
#else /* __PHX_DEBUG__ */
# define phx_printk(fmt, args...)
#endif /* __PHX_DEBUG__ */


#include <asm/processor.h>        // pt_regs

static __inline__ void check_stack(struct pt_regs *regs, const char *file,
				   const char *function, int line)
{
#ifdef CONFIG_DEBUG_STACKOVERFLOW
	unsigned long esp;
	extern void show_registers(struct pt_regs *regs);
	
	__asm__ __volatile__ ("l.addi %0,r1,0" : "=r" (esp));
	if(unlikely((esp & ~(TASK_SIZE - 1)) != (current_thread_info()->ksp & ~(TASK_SIZE - 1)))) {
		printk("%s:%d: %s(): stack switch, old: %lx, new: %lx\n",
		       file, line, function,
		       current_thread_info()->ksp, esp);
		show_registers(regs);
		show_stack(NULL, (unsigned long *)esp);
		__asm__ __volatile__("l.nop 1");
	}
		
	esp &= TASK_SIZE - 1;
	if (unlikely(esp < (sizeof(struct task_struct) + 1024))) {
		
		printk("%s:%d: %s(): stack overflow: %ld\n",
		       file, line, function,
		       esp - sizeof(struct task_struct));
		__asm__ __volatile__ ("l.addi %0,r1,%1" : "=r" (esp) : "K" (0));
		show_registers(regs);
		show_stack(NULL, (unsigned long *)esp);
		__asm__ __volatile__("l.nop 1");
	}
#endif
}

#endif /* __ASM_OPENRISC_HF_H */
