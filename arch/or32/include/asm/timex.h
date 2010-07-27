/*
 * linux/include/asm-or32/timex.h
 *
 * Based on:
 * include/asm-cris/timex.h
 */
#ifndef _OR32_TIMEX_H
#define _OR32_TIMEX_H

#include <asm/param.h>

//#define CLOCK_TICK_RATE	(CONFIG_OR32_SYS_CLK*1000000 / 4) /* Underlying HZ */
#define CLOCK_TICK_RATE	(CONFIG_OR32_SYS_CLK*1000000 / HZ) /* Underlying HZ */ // --jb
#if 0
#define CLOCK_TICK_FACTOR	20	/* Factor of both 1000000 and CLOCK_TICK_RATE */
#define FINETUNE ((((((long)LATCH * HZ - CLOCK_TICK_RATE) << SHIFT_HZ) * \
	(1000000/CLOCK_TICK_FACTOR) / (CLOCK_TICK_RATE/CLOCK_TICK_FACTOR)) \
		<< (SHIFT_SCALE-SHIFT_HZ)) / HZ)
#endif
/*
 * We don't have a cycle-counter.. but we do not support SMP anyway where this is
 * used so it does not matter.
 */

typedef unsigned int cycles_t;

static inline cycles_t get_cycles(void)
{
        return 0;
}

#define vxtime_lock()		do {} while (0)
#define vxtime_unlock()		do {} while (0)

#endif
