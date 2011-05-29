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
 */

#ifndef __ASM_OPENRISC_DELAY_H
#define __ASM_OPENRISC_DELAY_H

#include <linux/linkage.h>
#include <asm/param.h>

extern inline void __delay(int loops)
{
	__asm__ __volatile__ (
			      "l.srli %0,%0,1;"
			      "1: l.sfeqi %0,0;"
			      "l.bnf 1b;"
			      "l.addi %0,%0,-1;"
			      : "=r" (loops): "0" (loops));
}

/* Use only for very small delays ( < 1 msec).  */

extern unsigned long loops_per_jiffy;

extern inline void udelay(unsigned long usecs)
{
	__delay( usecs * loops_per_jiffy * HZ / 1000000 );
}

#endif 



