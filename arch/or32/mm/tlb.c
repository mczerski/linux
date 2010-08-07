/*
 *  linux/arch/or32/mm/tlb.c
 *
 *  or32 version
 *    author(s): Matjaz Breskvar (phoenix@bsemi.com)
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
 *  based on: linux/arch/cris/mm/tlb.c
 *    Copyright (C) 2000, 2001  Axis Communications AB
 *    Authors:   Bjorn Wesen (bjornw@axis.com)
 *
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/segment.h>
#include <asm/tlbflush.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>
#include <asm/or32-hf.h>
#include <asm/spr_defs.h>

#define D(x)

#define NUM_TLB_ENTRIES 64
#define TLB_OFFSET(add) (((add) >> PAGE_SHIFT) & (NUM_TLB_ENTRIES-1))

#define NO_CONTEXT -1

/* this is to work around for certian problems */
#define CONFIG_OR32_FLUSH_ALL

#define or32_disable_immu()                \
{                                          \
	unsigned long __t1, __t2;          \
	__asm__ __volatile__(              \
                "l.movhi  %6,hi(99f)    ;" \
		"l.ori    %6,%6,lo(99f) ;" \
                "l.movhi  %0,%5         ;" \
                "l.add    %0,%0,%6      ;" \
		"l.mtspr  r0,%0,%1      ;" \
		"l.mfspr  %0,r0,%2      ;" \
		"l.andi   %0,%0,lo(%3)  ;" \
		"l.mtspr  r0,%0,%4      ;" \
		"l.rfe   ;l.nop;l.nop;l.nop;l.nop;l.nop               ;" \
		"99:                     " \
		: "=r"(__t1)               \
                : "K"(SPR_EPCR_BASE), "K"(SPR_SR),        \
		  "K"(0x0000ffff&(~(SPR_SR_IME))), "K"(SPR_ESR_BASE), \
		  "K"((-KERNELBASE)>>16), "r"(__t2)); \
}

#define or32_enable_immu()            \
{                                     \
	unsigned long __t1;                \
	__asm__ __volatile__(         \
                "l.movhi  %0,hi(99f);" \
		"l.ori    %0,%0,lo(99f);" \
		"l.mtspr  r0,%0,%1  ;" \
		"l.mfspr  %0,r0,%2  ;" \
		"l.ori    %0,%0,lo(%3) ;" \
		"l.mtspr  r0,%0,%4  ;" \
		"l.rfe              ;" \
                "l.nop;l.nop;l.nop;l.nop;" \
		"99:                " \
		: "=r"(__t1)     \
		: "K"(SPR_EPCR_BASE), "K"(SPR_SR),         \
		  "K"(SPR_SR_IME), "K"(SPR_ESR_BASE));     \
}
 



/* invalidate all TLB entries */

void flush_tlb_all(void)
{
	int i;
	unsigned long flags;
	
	D(printk("tlb: flushed all\n"));
	
	local_irq_save(flags); /* flush needs to be atomic */
//	or32_disable_immu();

	for(i = 0; i < NUM_TLB_ENTRIES; i++) {
		mtspr_off(SPR_DTLBMR_BASE(0), i, 0);
		mtspr_off(SPR_ITLBMR_BASE(0), i, 0);
	}
	
//	or32_enable_immu();
	local_irq_restore(flags);
}

/* invalidate the selected mm context only */

void flush_tlb_mm(struct mm_struct *mm)
{
#ifdef CONFIG_OR32_FLUSH_ALL
	flush_tlb_all();
#else
        D(printk("tlb: flush mm (%p)\n", mm));
	
	if(mm->map_count) {
		struct vm_area_struct *mp;
		for(mp = mm->mmap; mp != NULL; mp = mp->vm_next)
			flush_tlb_range(mp, mp->vm_start,  mp->vm_end);
	}
#endif
}

/* invalidate a single page */

void flush_tlb_page(struct vm_area_struct *vma, 
		    unsigned long addr)
{
#ifdef CONFIG_OR32_FLUSH_ALL
	flush_tlb_all();
#else
	unsigned long tlb_offset, flags;

	D(printk("tlb: flush page %p \n", addr));
	
	addr &= PAGE_MASK; /* perhaps not necessary */
	tlb_offset = TLB_OFFSET(addr);
	
	local_irq_save(flags);  /* flush needs to be atomic */
	
	
	if((mfspr(SPR_DTLBMR_BASE(0) + tlb_offset) & PAGE_MASK) == addr) 
		mtspr(SPR_DTLBMR_BASE(0) + tlb_offset, 0);
	
	if((mfspr(SPR_ITLBMR_BASE(0) + tlb_offset) & PAGE_MASK) == addr)
		mtspr(SPR_ITLBMR_BASE(0) + tlb_offset, 0);
  
	local_irq_restore(flags);
#endif
}

/* invalidate a page range */

void flush_tlb_range(struct vm_area_struct *vma, 
		     unsigned long start,
		     unsigned long end)
{
#ifdef CONFIG_OR32_FLUSH_ALL
	flush_tlb_all();
#else
	unsigned long vpn, flags;
	
	D(printk("tlb: flush range %p<->%p (%p)\n",
		 start, end, vma));
	
	start = start >> PAGE_SHIFT;
	end   = end   >> PAGE_SHIFT;
	
	local_irq_save(flags);  /* flush needs to be atomic */
	
	for (vpn = start; vpn < end; vpn++) {
		unsigned long slot = vpn%NUM_TLB_ENTRIES;
		
		if (vpn == (mfspr(SPR_DTLBMR_BASE(0) + slot) >> PAGE_SHIFT)) {
			mtspr(SPR_DTLBMR_BASE(0) + slot,0);
			D(printk("DTLB invalidate :: vpn 0x%x, set %d\n", vpn, slot)); 
		}
		
		if (vpn == (mfspr(SPR_ITLBMR_BASE(0) + slot) >> PAGE_SHIFT)) {
			mtspr(SPR_ITLBMR_BASE(0) + slot,0);
			D(printk("ITLB invalidate :: vpn 0x%x, set %d\n", vpn, slot)); 
		}
	}
	local_irq_restore(flags);
#endif
}

/* called in schedule() just before actually doing the switch_to */

void switch_mm(struct mm_struct *prev, struct mm_struct *next,
	       struct task_struct *next_tsk)
{
        /* remember the pgd for the fault handlers
	 * this is similar to the pgd register in some other CPU's.
	 * we need our own copy of it because current and active_mm
	 * might be invalid at points where we still need to derefer
	 * the pgd.
	 */
	current_pgd = next->pgd;

	/* We don't have context support implemented, so flush all
	 * entries belonging to previous map
	 */

/*	
	phx_mmu("prev_mm %p, next_mm %p, next_tsk %p, "
		"next_tsk->mm %p, current %p", 
		prev, next, next_tsk, next_tsk ? next_tsk->mm : 0, current);
*/

#ifdef CONFIG_OR32_FLUSH_ALL
	flush_tlb_all();
#else
	if (prev != next)
		flush_tlb_mm(prev);
#endif
}

/*
 * Initialize the context related info for a new mm_struct
 * instance.
 */

int init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	mm->context = NO_CONTEXT;
	return 0;
}

/* called by __exit_mm to destroy the used MMU context if any before
 * destroying the mm itself. this is only called when the last user of the mm
 * drops it.
 */

void destroy_context(struct mm_struct *mm)
{
	D(printk("destroy_context %d (%p)\n", mm->context, mm));
	
#ifdef CONFIG_OR32_FLUSH_ALL
	flush_tlb_all();
#else
	flush_tlb_mm(mm);
#endif

}

/* called once during VM initialization, from init.c */

void __init tlb_init(void)
{
	/* invalidate the entire TLB */
	flush_tlb_all();
}
