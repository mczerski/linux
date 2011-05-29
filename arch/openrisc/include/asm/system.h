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

#ifndef __ASM_OPENRISC_SYSTEM_H
#define __ASM_OPENRISC_SYSTEM_H

#include <asm-generic/system.h>

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#define mtspr(_spr, _val) __asm__ __volatile__ ( 		\
	"l.mtspr r0,%1,%0"					\
	:: "K" (_spr), "r" (_val))
#define mtspr_off(_spr, _off, _val) __asm__ __volatile__ ( 	\
	"l.mtspr %0,%1,%2"					\
	:: "r" (_off), "r" (_val), "K" (_spr))

static inline unsigned long mfspr(unsigned long add)
{
	unsigned long ret;
	__asm__ __volatile__ ("l.mfspr %0,r0,%1" : "=r" (ret) : "K" (add));
	return ret;
}

static inline unsigned long mfspr_off(unsigned long add, unsigned long offset)
{
	unsigned long ret;
	__asm__ __volatile__ ("l.mfspr %0,%1,%2" : "=r" (ret)
						 : "r" (offset), "K" (add));
	return ret;
}

/* We probably need this definition, but the generic system.h provides it
 * and it's not used on our arch anyway...
 */
/*#define nop() __asm__ __volatile__ ("l.nop"::)*/

#endif /* __ASSEMBLY__ */
#endif /* __KERNEL__ */
#endif /* __ASM_OPENRISC_SYSTEM_H */

