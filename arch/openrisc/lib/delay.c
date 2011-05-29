/*
 * OpenRISC delay.c
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source 
 * declaration.
 *
 * Modifications for the OpenRISC architecture:
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      version 2 as published by the Free Software Foundation
 *
 * Precise Delay Loops
 */
 
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/timex.h>
#include <linux/param.h>
#include <linux/types.h>
#include <linux/init.h>

#include <asm/processor.h>
#include <asm/system.h>

int __devinit read_current_timer(unsigned long *timer_value)
{
	u32 v = mfspr(SPR_TTCR);
//	printk("timer value = %d\n", v);
	*timer_value = v;
//	*timer_value = mfspr(SPR_TTCR);
	return 0;
}

/*FIXME: move things from delay.h to this file */

/*
void __delay(unsigned long loops)
{
	unsigned bclock, now;

	bclock = mfspr(SPR_TTCR);
	do {
		now = mfspr(SPR_TTCR);
	} while ((now - bclock) < loops);
}
*/
/*
inline void __const_udelay(unsigned long xloops)
{
	unsigned long long loops;

	asm("mulu.d %0, %1, %2"
	    : "=r"(loops)
	    : "r"(current_cpu_data.loops_per_jiffy * HZ), "r"(xloops));
	__delay(loops >> 32);
}
*/


//void __udelay(unsigned long usecs)
//{
//	__const_udelay(usecs * 0x000010c7); /* 2**32 / 1000000 (rounded up) */
//}

//void __ndelay(unsigned long nsecs)
//{
//	__const_udelay(nsecs * 0x00005); /* 2**32 / 1000000000 (rounded up) */
//}
