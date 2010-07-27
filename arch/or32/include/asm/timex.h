#ifndef _OR32_TIMEX_H
#define _OR32_TIMEX_H

#include <asm-generic/timex.h>

#include <asm/param.h>

#define CLOCK_TICK_RATE	(CONFIG_OR32_SYS_CLK*1000000 / HZ)

#define ARCH_HAS_READ_CURRENT_TIMER

#endif
