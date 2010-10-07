#ifdef __KERNEL__
#ifndef __OR32_IRQ_H__
#define __OR32_IRQ_H__

#include <linux/interrupt.h>
#include <asm/machdep.h>
#include <asm/atomic.h>

extern void disable_irq(unsigned int);
extern void disable_irq_nosync(unsigned int);
extern void enable_irq(unsigned int);

#define NO_IRQ		(0)

#define	NR_IRQS		32

static inline int irq_canonicalize(int irq)
{
	return(irq);
}

/*
 * "Generic" interrupt sources
 */

//#define IRQ_UART_0            (2)       /* interrupt source for UART dvice 0 */
//#define IRQ_ETH_0             (4)       /* interrupt source for Ethernet dvice 0 */
#define IRQ_PS2_0             (5)       /* interrupt source for ps2 dvice 0 */
#define IRQ_SCHED_TIMER       (0)       /* interrupt source for scheduling timer */

/*
 * various flags for request_irq()
 */
#define IRQ_FLG_LOCK    (0x0001)        /* handler is not replaceable   */
#define IRQ_FLG_REPLACE (0x0002)        /* replace existing handler     */
#define IRQ_FLG_PRI_HI  (0x0004)
#define IRQ_FLG_STD     (0x8000)        /* internally used              */

/*
 * This structure has only 4 elements for speed reasons
 */
struct irq_handler {
	irqreturn_t     (*handler)(int, void *);
	unsigned long   flags;
	void            *dev_id;
	const char      *devname;
};

/** FIXME - not implement 
 * irq_dispose_mapping - Unmap an interrupt 
 * @virq: linux virq number of the interrupt to unmap 
 */ 
static inline void irq_dispose_mapping(unsigned int virq) 
{ 
        return; 
}

#endif /* __OR32_IRQ_H__ */
#endif /* __KERNEL__ */
