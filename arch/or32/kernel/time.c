/*
 *  linux/arch/or32/kernel/time.c
 *
 *  or32 version
 *
 *  For more information about OpenRISC processors, licensing and
 *  design services you may contact Beyond Semiconductor at
 *  sales@bsemi.com or visit website http://www.bsemi.com.
 *
 *  Copied/hacked from:
 *
 *  linux/arch/m68knommu/kernel/time.c
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *
 * This file contains the m68k-specific time handling details.
 * Most of the stuff is located in the machine specific files.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/profile.h>
#include <linux/mm.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/interrupt.h>

#include <asm/machdep.h>
#include <asm/segment.h>
#include <asm/io.h>
#include <asm/or32-hf.h>

#define TICK_SIZE (tick_nsec / 1000)

static inline int set_rtc_mmss(unsigned long nowtime)
{
  if (mach_set_clock_mmss)
    return mach_set_clock_mmss (nowtime);
  return -1;
}

/* last time the RTC clock got updated */
static long last_rtc_update;


/*
 * This get called at every clock tick...
 *
 */
irqreturn_t timer_interrupt(struct pt_regs * regs)
{
#if 0
  	check_stack(regs, __FILE__, __FUNCTION__, __LINE__);
#endif
        /*
         * Here we are in the timer irq handler. We just have irqs locally
         * disabled but we don't know if the timer_bh is running on the other
         * CPU. We need to avoid to SMP race with it. NOTE: we don' t need
         * the irq version of write_lock because as just said we have irq
         * locally disabled. -arca
         */
	/*profile_tick(CPU_PROFILING); broken on or32 why? RGD*/
	
	if (mach_tick) /*Not sure why we need to do this RGD*/
	  mach_tick();
	
	do_timer(1); /*RGD*/
	
#ifndef CONFIG_SMP
	update_process_times(user_mode(regs));
#endif
        return IRQ_HANDLED;
}




/*
 * Scheduler clock - returns current time in nanosec units.
 */
unsigned long long sched_clock(void)
{
	return (unsigned long long)jiffies * (1000000000 / HZ);
}


void __init time_init(void)
{
	unsigned int year, mon, day, hour, min, sec;

	extern void arch_gettod(int *year, int *mon, int *day, int *hour,
				int *min, int *sec);

	year = 1980;
	mon = day = 1;
	hour = min = sec = 0;
	arch_gettod (&year, &mon, &day, &hour, &min, &sec);

	if ((year += 1900) < 1970)
		year += 100;

	
	if (mach_sched_init)
		mach_sched_init();
}

