/*
 * OpenRISC IRQ
 *
 * Copyright (C) 2010 Jonas Bonn <jonas@southpole.se>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
*/

#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/ftrace.h>
#include <linux/irq.h>
#include <linux/seq_file.h>
#include <linux/kernel_stat.h>

#include <linux/irqflags.h>

/* read interrupt enabled status */
unsigned long arch_local_save_flags(void) {
	return (mfspr(SPR_SR) & (SPR_SR_IEE|SPR_SR_TEE));
}
EXPORT_SYMBOL(arch_local_save_flags);

/* set interrupt enabled status */
void arch_local_irq_restore(unsigned long flags) {
	mtspr(SPR_SR, ((mfspr(SPR_SR) & ~(SPR_SR_IEE|SPR_SR_TEE)) | flags));
}
EXPORT_SYMBOL(arch_local_irq_restore);


/* OR1K PIC implementation */

static const char const * irq_name[NR_IRQS] = {
	"int0", "int1", "int2", "int3",	"int4", "int5", "int6", "int7",
	"int8", "int9", "int10", "int11", "int12", "int13", "int14", "int15",
	"int16", "int17", "int18", "int19", "int20", "int21", "int22", "int23",
	"int24", "int25", "int26", "int27", "int28", "int29", "int30", "int31",
};

static void pic_mask(struct irq_data *data)
{
	mtspr(SPR_PICMR, mfspr(SPR_PICMR) & ~(1UL << data->irq));
}

static void pic_unmask(struct irq_data *data)
{
	mtspr(SPR_PICMR, mfspr(SPR_PICMR) | (1UL << data->irq));
}

static void pic_ack(struct irq_data *data)
{
	/* EDGE-triggered interrupts need to be ack'ed in order to clear
	 * the latch.  
	 * LEVER-triggered interrupts do not need to be ack'ed; however, 
	 * ack'ing the interrupt has no ill-effect and is quicker than
	 * trying to figure out what type it is...
	 */

	/* FIXME: This is contrary to spec which says write 1 to ack
	 * interrupt... */
//	mtspr(SPR_PICSR, (1UL << irq)); 
	mtspr(SPR_PICSR, mfspr(SPR_PICSR) & ~(1UL << data->irq));
}

static void pic_mask_ack(struct irq_data *data)
{
	/* Comment for pic_ack applies here, too */

	mtspr(SPR_PICMR, mfspr(SPR_PICMR) & ~(1UL << data->irq));
	/* FIXME: This is contrary to spec which says write 1 to ack
	 * interrupt... */
//	mtspr(SPR_PICSR, (1UL << irq)); 
	mtspr(SPR_PICSR, mfspr(SPR_PICSR) & ~(1UL << data->irq));
}

static int pic_set_type(struct irq_data *data, unsigned int flow_type) {
	/* There's nothing to do in the PIC configuration when changing
	 * flow type.  Level and edge-triggered interrupts are both
	 * supported, but it's PIC-implementation specific which type
	 * is handled. */

	return 0;
}

static inline int pic_get_irq(void)
{
	int irq;

	irq = ffs(mfspr(SPR_PICSR));

	return irq ? irq - 1 : NO_IRQ;
}

static struct irq_chip or1k_pic = {
	.name = "or1k-PIC",
	.irq_unmask = pic_unmask,
	.irq_mask = pic_mask,
	.irq_ack = pic_ack,
	.irq_mask_ack = pic_mask_ack,
	.irq_set_type = pic_set_type
};

void __init init_IRQ(void)
{
	int i;

	/* Setup IRQ descriptors... these all default to LEVEL triggered
	 * so if EDGE triggered is needed then this needs to be fixed
	 * up.
	 */
	for (i = 0; i < NR_IRQS; i++) {
		irq_set_chip_and_handler_name(i, &or1k_pic,
			handle_level_irq, irq_name[i]);
/*		irq_desc[i].status |= IRQ_LEVEL;*/
	}

	/* Disable all interrupts until explicitly requested */
	mtspr(SPR_PICMR, (0UL));
}

void __irq_entry do_IRQ(struct pt_regs *regs)
{
	int irq;
	struct pt_regs *old_regs = set_irq_regs(regs);

	irq_enter();

	/* FIXME: This loop always handles the lowest interrupt
	 * first and seems to be able to lead to starvation for
	 * handlers with higher numbered IRQ's */
	while ((irq = pic_get_irq()) != NO_IRQ) {
		generic_handle_irq(irq);
	}

        irq_exit();
        set_irq_regs(old_regs);
}

unsigned int irq_create_of_mapping(struct device_node *controller,
                                   const u32 *intspec, unsigned int intsize)
{
        return intspec[0];
}
EXPORT_SYMBOL_GPL(irq_create_of_mapping);
