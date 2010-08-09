#ifndef _OR32_TIMEX_H
#define _OR32_TIMEX_H

#include <asm-generic/timex.h>

#include <asm/param.h>
#include <asm/cpuinfo.h>

//#define CLOCK_TICK_RATE	(CONFIG_OR32_SYS_CLK*1000000 / HZ)
//#define CLOCK_TICK_RATE	(cpuinfo.clock_frequency / HZ)

/* This isn't really used any more */
#define CLOCK_TICK_RATE 1000

#define ARCH_HAS_READ_CURRENT_TIMER

#endif
