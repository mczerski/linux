/*
 * OpenRISC Linux
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

#include <linux/init.h>
#include <asm/system.h>

int __devinit read_current_timer(unsigned long *timer_value)
{
	*timer_value = mfspr(SPR_TTCR);
	return 0;
}

/*FIXME: move things from delay.h to this file */
