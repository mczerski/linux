/*
 *  linux/arch/or32/kernel/idle.c
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
 * Idle daemon for or32.  Idle daemon will handle any action
 * that needs to be taken when the system becomes idle.
 *
 * Based on:
 * arch/ppc/kernel/idle.c
 * Written by Cort Dougan (cort@cs.nmt.edu)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/tick.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/cache.h>
#include <asm/pgalloc.h>

void (*powersave)(void) = NULL;

static inline void pm_idle(void) {
	barrier();
}

void cpu_idle(void)
{
        unsigned int cpu = smp_processor_id();

        set_thread_flag(TIF_POLLING_NRFLAG);

        /* endless idle loop with no priority at all */
        while (1) {
                tick_nohz_stop_sched_tick(1);

                while (!need_resched()) {
                        check_pgt_cache();
                        rmb();

/*                        if (cpu_is_offline(cpu))
                                play_dead();
*/
			clear_thread_flag(TIF_POLLING_NRFLAG);

                        local_irq_disable();
                        /* Don't trace irqs off for idle */
                        stop_critical_timings();
			if (powersave != NULL )
				powersave();
			start_critical_timings();
			local_irq_enable();
		        set_thread_flag(TIF_POLLING_NRFLAG);
                }

                tick_nohz_restart_sched_tick();
                preempt_enable_no_resched();
                schedule();
                preempt_disable();
        }
}
