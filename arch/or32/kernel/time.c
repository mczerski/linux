/*
 *  linux/arch/or32/kernel/time.c
 *
 *  Copyright (C) 2010 Jonas Bonn 
 *
 */

//#define DEBUG 1

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

#include <linux/clocksource.h>
#include <linux/clockchips.h>

#include <asm/irq_regs.h>

#include <asm/cpuinfo.h>
#include <asm/segment.h>
#include <asm/io.h>
#include <asm/or32-hf.h>


static inline void or32_timer_stop(void)
{
	mtspr(SPR_TTMR, 0);
}

static int or32_timer_set_next_event(unsigned long delta,
                                        struct clock_event_device *dev)
{
/*	static int i = 0;
	static u32 x[32];*/
	u32 c;

//        printk("%s: next event, delta %ld\n", __func__, (u32)delta);

/*	x[i++] = delta;
	if (i == 32) {
		for (i = 0; i < 32; i++) {
			printk("jd: %ld\n", x[i]);
		}
		i = 0;
	}
*/
	/* Read the 32 bit counter value,  mask off the low 28 bits, add delta */
	c = mfspr(SPR_TTCR);
	c &= SPR_TTMR_PERIOD;
	c += delta;
	c &= SPR_TTMR_PERIOD;

	/* Set counter and enable interrupt; keep timer in continuous mode always */
	mtspr(SPR_TTMR, SPR_TTMR_CR | SPR_TTMR_IE | c);

	c = mfspr(SPR_TTMR);
	pr_debug("SPR_TTMR = %x\n", c);

        return 0;
}

static void or32_timer_set_mode(enum clock_event_mode mode,
                                struct clock_event_device *evt)
{
        switch (mode) {
        case CLOCK_EVT_MODE_PERIODIC:
                printk(KERN_INFO "%s: periodic\n", __func__);
		BUG();
//                or32_timer_start_periodic(cpuinfo.freq_div_hz);
                break;
        case CLOCK_EVT_MODE_ONESHOT:
                printk(KERN_INFO "%s: oneshot\n", __func__);
                break;
        case CLOCK_EVT_MODE_UNUSED:
                printk(KERN_INFO "%s: unused\n", __func__);
                break;
        case CLOCK_EVT_MODE_SHUTDOWN:
                printk(KERN_INFO "%s: shutdown\n", __func__);
//                or32_timer_stop();
                break;
        case CLOCK_EVT_MODE_RESUME:
                printk(KERN_INFO "%s: resume\n", __func__);
                break;
        }
}

/* This is the clock event device based on the OR32 timer.
 * As the timer is being used as a continuous clock-source (required for HR timers) 
 * we cannot enable the PERIODIC feature.  The tick timer can run using one-shot
 * events, so no problem.
 */

static struct clock_event_device clockevent_or32_timer = {
        .name           = "or32_timer_clockevent",
        .features       = CLOCK_EVT_FEAT_ONESHOT,
//        .shift          = 36,
        .rating         = 300,
        .set_next_event = or32_timer_set_next_event,
        .set_mode       = or32_timer_set_mode,
};

static inline void timer_ack(void)
{
	/* Clear the IP bit and disable further interrupts */
//	mtspr(SPR_TTMR, mfspr(SPR_TTMR) | ~(SPR_TTMR_IE | SPR_TTMR_IP));

	/* Clear the IP bit and disable further interrupts */
	/* This can be done very simply... just just need to keep the timer
	   running, so just maintain the CR bits while clearing the rest 
	   of the register
	*/
	mtspr(SPR_TTMR, SPR_TTMR_CR);
	pr_debug("TIMER ACKED TTMR=%lx\n", mfspr(SPR_TTMR));
}

/*
 * The timer interrupt is mostly handled in generic code nowadays... this
 * function just acknowledges the interrupt and fires the event handler that 
 * has been set on the clockevent device by the generic time management code.
 *
 * This function needs to be called by the timer exception handler and that's
 * all the exception handler needs to do.
 *
 * FIXME: Check how to get this into a threaded interrupt handler or something...
 */

/* FIXME: what's with the pt_regs parameter here... really needed? */
/* FIXME; should this really be marked __irq_entry */

irqreturn_t timer_interrupt(struct pt_regs * regs)
{
        struct pt_regs *old_regs = set_irq_regs(regs);
        struct clock_event_device *evt = &clockevent_or32_timer;
/*#ifdef CONFIG_HEART_BEAT
        heartbeat();
#endif
*/
	pr_debug("JB\n");

        timer_ack();

	/*
	 * update_process_times() expects us to have called irq_enter().
	 */

	irq_enter();
        evt->event_handler(evt);
	irq_exit();

        set_irq_regs(old_regs);

        return IRQ_HANDLED;
}

/*
static struct irqaction timer_irqaction = {
        .handler = timer_interrupt,
        .flags = IRQF_DISABLED | IRQF_TIMER,
        .name = "timer",
        .dev_id = &clockevent_or32_timer,
};
*/
static __init void or32_clockevent_init(void)
{
	clockevents_calc_mult_shift(&clockevent_or32_timer, cpuinfo.clock_frequency, 4);
     	printk("TIMER INIT event **************** ******* mult = %d\n", clockevent_or32_timer.mult);
	printk("TIMER INIT  event ***********************shift = %d\n", clockevent_or32_timer.shift);
//   clockevent_or32_timer.mult =
//                div_sc(cpuinfo.clock_frequency, NSEC_PER_SEC,
//                                clockevent_or32_timer.shift);
	/* We only have 28 bits */
        clockevent_or32_timer.max_delta_ns =
                clockevent_delta2ns((u32)0x0fffffff, &clockevent_or32_timer);
	printk("MAX DELTA = %lld\n", clockevent_or32_timer.max_delta_ns);
        clockevent_or32_timer.min_delta_ns =
                clockevent_delta2ns(1, &clockevent_or32_timer);
	printk("MIN DELTA = %lld\n", clockevent_or32_timer.min_delta_ns);
        clockevent_or32_timer.cpumask = cpumask_of(0);
        clockevents_register_device(&clockevent_or32_timer);

	printk("clock event registered\n");
}













/** 
 * Incrementer
 */


static cycle_t or32_timer_read(struct clocksource* cs) {
//	printk("timer read %lx\n", mfspr(SPR_TTCR));

	return (cycle_t) mfspr(SPR_TTCR);
} 

static struct clocksource or32_timer = {
	.name		= "or32_timer",
        .rating         = 200,
	.read		= or32_timer_read,
        .mask           = CLOCKSOURCE_MASK(32),
//	.shift		= 16,
        .flags          = CLOCK_SOURCE_IS_CONTINUOUS,
};

static int __init or32_timer_init(void)
{

	clocksource_calc_mult_shift(&or32_timer, cpuinfo.clock_frequency, 4);

//        or32_timer.mult = 
//		clocksource_hz2mult(cpuinfo.clock_frequency, or32_timer.shift);
	printk("clock frequency = %d\n", cpuinfo.clock_frequency);
	printk("TIMER INIT *********************** mult = %d\n", or32_timer.mult);
	printk("TIMER INIT *********************** mult = %d\n", or32_timer.shift);
        if (clocksource_register(&or32_timer))
                panic("failed to register clocksource");

        /* Set counter period, enable timer and interrupt */

	/* Enable the incrementer in 'continuous' mode with interrupt disabled */
        mtspr(SPR_TTMR, SPR_TTMR_CR);

	/* FIXME: We probably want a timercounter, too */
        /* register timecounter - for ftrace support */
//        init_microblaze_timecounter();
        return 0;
}

//arch_initcall(or32_incrementer_init);

void __init time_init(void)
{
	u32 upr;

	upr = mfspr(SPR_UPR);
	if (!(upr & SPR_UPR_TTP))
		panic("Linux not supported on devices without tick timer");

	or32_timer_init();
	or32_clockevent_init();
}
