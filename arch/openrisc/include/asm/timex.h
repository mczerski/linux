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

#ifndef __ASM_OPENRISC_TIMEX_H
#define __ASM_OPENRISC_TIMEX_H

#include <asm-generic/timex.h>

#include <asm/param.h>
#include <asm/cpuinfo.h>

//#define CLOCK_TICK_RATE	(CONFIG_OPENRISC_SYS_CLK*1000000 / HZ)
//#define CLOCK_TICK_RATE	(cpuinfo.clock_frequency / HZ)

/* This isn't really used any more */
#define CLOCK_TICK_RATE 1000

#define ARCH_HAS_READ_CURRENT_TIMER

#endif
