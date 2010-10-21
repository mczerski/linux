/*
 *  linux/arch/or32/kernel/irq.c
 *
 *  or32 version
 *    author(s): Matjaz Breskvar (phoenix@bsemi.com)
 *               Jonas Bonn (jonas@southpole.se)
 *
 *  For more information about OpenRISC processors, licensing and
 *  design services you may contact Beyond Semiconductor at
 *  sales@bsemi.com or visit website http://www.bsemi.com.
 *
 *  derived from cris, i386, m68k, ppc, sh ports.
 *
 *  changes:
 *  18. 11. 2003: Matjaz Breskvar (phoenix@bsemi.com)
 *    initial port to or32 architecture
 *
 */

#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/ftrace.h>
#include <linux/irq.h>

#include <asm/irqflags.h>

/* read interrupt enabled status */
unsigned long __raw_local_save_flags(void) {
//	printk("JONAS; raw_local_save_flags\n");
	return (mfspr(SPR_SR) & (SPR_SR_IEE|SPR_SR_TEE));
}
EXPORT_SYMBOL(__raw_local_save_flags);

/* set interrupt enabled status */
void raw_local_irq_restore(unsigned long flags) {
//	printk("JONAS; raw_local_irq_restore\n");
	mtspr(SPR_SR, ((mfspr(SPR_SR) & ~(SPR_SR_IEE|SPR_SR_TEE)) | flags));
}
EXPORT_SYMBOL(raw_local_irq_restore);





/* OR1K PIC implementation */

static const char const * irq_name[NR_IRQS] = {
	"int0", "int1", "int2", "int3",	"int4", "int5", "int6", "int7",
	"int8", "int9", "int10", "int11", "int12", "int13", "int14", "int15",
	"int16", "int17", "int18", "int19", "int20", "int21", "int22", "int23",
	"int24", "int25", "int26", "int27", "int28", "int29", "int30", "int31",
};

void pic_mask(unsigned int irq)
{
	mtspr(SPR_PICMR, mfspr(SPR_PICMR) & ~(1UL << irq));
}

void pic_unmask(unsigned int irq)
{
	mtspr(SPR_PICMR, mfspr(SPR_PICMR) | (1UL << irq));
}

void pic_ack(unsigned int irq)
{
	/* EDGE-triggered interrupts need to be ack'ed in order to clear
	 * the latch.  
	 * LEVER-triggered interrupts do not need to be ack'ed; however, 
	 * ack'ing the interrupt has no ill-effect and is quicked than
	 * trying to figure out what type it is...
	 */

	/* FIXME: This is contrary to spec which says write 1 to ack
	 * interrupt... */
//	mtspr(SPR_PICSR, (1UL << irq)); 
	mtspr(SPR_PICSR, mfspr(SPR_PICSR) & ~(1UL << irq));
}

void pic_mask_ack(unsigned int irq)
{
	/* Comment for pic_ack applies here, too */

	mtspr(SPR_PICMR, mfspr(SPR_PICMR) & ~(1UL << irq));
	/* FIXME: This is contrary to spec which says write 1 to ack
	 * interrupt... */
//	mtspr(SPR_PICSR, (1UL << irq)); 
	mtspr(SPR_PICSR, mfspr(SPR_PICSR) & ~(1UL << irq));
}

int pic_set_type(unsigned int irq, unsigned int flow_type) {
	/* There's nothing to do in the PIC configuration when changing
	 * flow type.  Level and edge-triggered interrupts are both
	 * supported, but it's PIC-implementation specific which type
	 * is handled. */

	return 0;
}

int pic_get_irq()
{
	int irq;
	int i;
	unsigned long mask;
	unsigned long pend = mfspr(SPR_PICSR);

	/* Bail if no IRQ pending */
	if (pend == 0)
		return -1;

//	printk("Jonas IRQ\n");
//	printk("pend = 0x%lx\n", pend);

	i = 16;
	irq = 0;
	mask = (1UL << i) -1;
	while (i > 0) {
//		printk("JOnas IRQ trest\n");
		if (!(pend & mask)) {
			pend >>= i;
			irq += i;
//			printk("New pend = 0x%lx\n", pend);
		}
		i >>= 1;
//		printk("New i = %d\n", i);
		mask >>= i;
	}

//	printk("Got IRQ %d\n", irq);

	return irq;
#if 0
	irq = 0;
	while (!(pend & 1UL)) {
		irq++;
		pend >>= 1;
	}


	int irq;
	int mask;

	unsigned long pend = mfspr(SPR_PICSR) & 0xfffffffc;



	if (pend & 0x0000ffff) {
		if (pend & 0x000000ff) {
			if (pend & 0x0000000f) {
				mask = 0x00000001;
				irq = 0;
			} else {
				mask = 0x00000010;
				irq = 4;
			}
		} else {
			if (pend & 0x00000f00) {
				mask = 0x00000100;
				irq = 8;
			} else {
				mask = 0x00001000;
				irq = 12;
			}
		}
	} else if(pend & 0xffff0000) {
		if (pend & 0x00ff0000) {
			if (pend & 0x000f0000) {
				mask = 0x00010000;
				irq = 16;
			} else {
				mask = 0x00100000;
				irq = 20;
			}
		} else {
			if (pend & 0x0f000000) {
				mask = 0x01000000;
				irq = 24;
			} else {
				mask = 0x10000000;
				irq = 28;
			}
		}
	} else {
		return -1;
	}

	while (! (mask & pend)) {
		mask <<=1;
		irq++;
	}

//	mtspr(SPR_PICSR, mfspr(SPR_PICSR) & ~mask);
	return irq;

#endif
}

/**
 * struct irq_chip - hardware interrupt chip descriptor
 *
 * @name:               name for /proc/interrupts
 * @startup:            start up the interrupt (defaults to ->enable if NULL)
 * @shutdown:           shut down the interrupt (defaults to ->disable if NULL)
 * @enable:             enable the interrupt (defaults to chip->unmask if NULL)
 * @disable:            disable the interrupt
 * @ack:                start of a new interrupt
 * @mask:               mask an interrupt source
 * @mask_ack:           ack and mask an interrupt source
 * @unmask:             unmask an interrupt source
 * @eoi:                end of interrupt - chip level
 * @end:                end of interrupt - flow level
 * @set_affinity:       set the CPU affinity on SMP machines
 * @retrigger:          resend an IRQ to the CPU
 * @set_type:           set the flow type (IRQ_TYPE_LEVEL/etc.) of an IRQ
 * @set_wake:           enable/disable power-management wake-on of an IRQ
 *
 * @bus_lock:           function to lock access to slow bus (i2c) chips
 * @bus_sync_unlock:    function to sync and unlock slow bus (i2c) chips
 *
 * @release:            release function solely used by UML
 * @typename:           obsoleted by name, kept as migration helper
 */

/* This is the optional PIC for the OR*/

static struct irq_chip or1k_pic = {
	.name = "OR1K PIC",
	.unmask = pic_unmask,
	.mask = pic_mask,
	.ack = pic_ack,
	.mask_ack = pic_mask_ack,
	.set_type = pic_set_type
};

void __init init_IRQ(void)
{
	int i;

	/* Setup IRQ descriptors... these all default to LEVEL triggered
	 * so if EDGE triggered is needed then this needs to be fixed
	 * up.
	 */

	for (i = 0; i < NR_IRQS; i++) {
		set_irq_chip_and_handler_name(i, &or1k_pic,
			handle_level_irq, irq_name[i]);
		irq_desc[i].status |= IRQ_LEVEL;
	}

	/* Disable all interrupts until explicitly requested */
	mtspr(SPR_PICMR, (0UL));
}

int show_interrupts(struct seq_file *p, void *v)
{
	/* FIXME */
#if 0
	int i = *(loff_t *) v;
	struct irqaction * action;
	unsigned long flags;
	
	if (i < NR_IRQS) {
                local_irq_save(flags);
		action = irq_action[i];
                if (!action) 
                        goto skip;
		seq_printf(p, "%2d: %10u %c %s",
			   i, kstat_cpu(0).irqs[i],
			   (action->flags & SA_INTERRUPT) ? '+' : ' ',
			   action->name);
                for (action = action->next; action; action = action->next) {
                        seq_printf(p, ",%s %s",
				   (action->flags & SA_INTERRUPT) ? " +" : "",
				   action->name);
                }
		seq_putc(p, '\n');
	skip:
                local_irq_restore(flags);
        }
#endif
        return 0;
}

void __irq_entry do_IRQ(struct pt_regs *regs)
{
	int irq;
	struct pt_regs *old_regs = set_irq_regs(regs);

	irq_enter();

	while ((irq = pic_get_irq()) >= 0) {
		generic_handle_irq(irq);
	}

        irq_exit();
        set_irq_regs(old_regs);
}

#if 0
int request_irq(unsigned int irq,
		irqreturn_t (*handler)(int, void *), /*RGD removed pt_reg*/
		unsigned long flags, const char *devname, void *dev_id)
{
	if (irq >= NR_IRQS) {
		printk("%s: Incorrect IRQ %d from %s\n", __FUNCTION__, irq, devname);
		return -ENXIO;
	}

	if (!(irq_list[irq].flags & IRQ_FLG_STD)) {
		if (irq_list[irq].flags & IRQ_FLG_LOCK) {
			printk("%s: IRQ %d from %s is not replaceable\n",
			       __FUNCTION__, irq, irq_list[irq].devname);
			return -EBUSY;
		}
		if (flags & IRQ_FLG_REPLACE) {
			printk("%s: %s can't replace IRQ %d from %s\n",
			       __FUNCTION__, devname, irq, irq_list[irq].devname);
			return -EBUSY;
		}
	}
	irq_list[irq].handler = handler;
	irq_list[irq].flags   = flags;
	irq_list[irq].dev_id  = dev_id;
	irq_list[irq].devname = devname;

	pic_enable_irq(irq);
	
	return 0;
}

void free_irq(unsigned int irq, void *dev_id)
{
	if (irq >= NR_IRQS) {
		printk("%s: Incorrect IRQ %d\n", __FUNCTION__, irq);
		return;
	}

	pic_disable_irq(irq);

	irq_list[irq].handler = NULL;
	irq_list[irq].flags   = IRQ_FLG_STD;
	irq_list[irq].dev_id  = NULL;
	irq_list[irq].devname = default_names[irq];
}

unsigned long probe_irq_on (void)
{
	return 0;
}

int probe_irq_off (unsigned long irqs)
{
	return 0;
}

void enable_irq(unsigned int irq)
{
	if (irq >= NR_IRQS) {
		printk("%s: Incorrect IRQ %d\n", __FUNCTION__, irq);
		return;
	}
	pic_enable_irq(irq);
}

void disable_irq(unsigned int irq)
{
	if (irq >= NR_IRQS) {
		printk("%s: Incorrect IRQ %d\n", __FUNCTION__, irq);
		return;
	}
	pic_disable_irq(irq);
}

void disable_irq_nosync(unsigned int irq)
{
        disable_irq(irq);
}
#endif
#if 0
int get_irq_list(char *buf)
{
	int i, len = 0;

	/* autovector interrupts */
	for (i = 0; i < NR_IRQS; i++) {
		if (irq_list[i].handler) {
			if (irq_list[i].flags & IRQ_FLG_LOCK)
				len += sprintf(buf+len, "L ");
			else
				len += sprintf(buf+len, "  ");
			if (irq_list[i].flags & IRQ_FLG_PRI_HI)
				len += sprintf(buf+len, "H ");
			else
				len += sprintf(buf+len, "L ");
			len += sprintf(buf+len, "%s\n", irq_list[i].devname);
		}
	}

	return len;
}
#endif
#if 0
void dump(struct pt_regs *fp)
{
	unsigned long	*sp;
	unsigned char	*tp;
	int		i;

	printk("\nCURRENT PROCESS:\n\n");
	printk("COMM=%s PID=%d\n", current->comm, current->pid);
	if (current->mm) {
		printk("TEXT=%08x-%08x DATA=%08x-%08x BSS=%08x-%08x\n",
			(int) current->mm->start_code,
			(int) current->mm->end_code,
			(int) current->mm->start_data,
			(int) current->mm->end_data,
			(int) current->mm->end_data,
			(int) current->mm->brk);
		printk("USER-STACK=%08x  KERNEL-STACK=%08x\n\n",
			(int) current->mm->start_stack,
			(int) current->kernel_stack_page);
	}
	printk("PC: %08lx  Status: %08lx\n",
	       fp->pc, fp->sr);
	printk("R0 : %08lx  %08lx  %08lx  %08lx  %08lx  %08lx  %08lx  %08lx\n",
	       	0L,             fp->sp,      fp->gprs[0], fp->gprs[1], 
		fp->gprs[2], fp->gprs[3], fp->gprs[4], fp->gprs[5]);
	printk("R8 : %08lx  %08lx  %08lx  %08lx  %08lx  %08lx  %08lx  %08lx\n",
	       	fp->gprs[6], fp->gprs[7], fp->gprs[8], fp->gprs[9], 
		fp->gprs[10], fp->gprs[11], fp->gprs[12], fp->gprs[13]);
	printk("R16: %08lx  %08lx  %08lx  %08lx  %08lx  %08lx  %08lx  %08lx\n",
	       	fp->gprs[14], fp->gprs[15], fp->gprs[16], fp->gprs[17], 
		fp->gprs[18], fp->gprs[19], fp->gprs[20], fp->gprs[21]);
	printk("R24: %08lx  %08lx  %08lx  %08lx  %08lx  %08lx  %08lx  %08lx\n",
	       	fp->gprs[22], fp->gprs[23], fp->gprs[24], fp->gprs[25], 
		fp->gprs[26], fp->gprs[27], fp->gprs[28], fp->gprs[29]);

	printk("\nUSP: %08lx   TRAPFRAME: %08x\n",
		fp->sp, (unsigned int) fp);

	printk("\nCODE:");
	tp = ((unsigned char *) fp->pc) - 0x20;
	for (sp = (unsigned long *) tp, i = 0; (i < 0x40);  i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n");

	printk("\nKERNEL STACK:");
	tp = ((unsigned char *) fp) - 0x40;
	for (sp = (unsigned long *) tp, i = 0; (i < 0xc0); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n");
	if (STACK_MAGIC != *(unsigned long *)current->kernel_stack_page)
                printk("(Possibly corrupted stack page??)\n");
	printk("\n");

	printk("\nUSER STACK:");
	tp = (unsigned char *) (fp->sp - 0x10);
	for (sp = (unsigned long *) tp, i = 0; (i < 0x80); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n\n");
}
#endif /* 0 */
/*
void init_irq_proc(void)
{
	phx_warn("TODO");
}
*/
/*
unsigned int irq_create_mapping(struct irq_host *host, irq_hw_number_t hwirq)
{
        return hwirq;
}
EXPORT_SYMBOL_GPL(irq_create_mapping);
*/

unsigned int irq_create_of_mapping(struct device_node *controller,
                                   const u32 *intspec, unsigned int intsize)
{
        return intspec[0];
}
EXPORT_SYMBOL_GPL(irq_create_of_mapping);
