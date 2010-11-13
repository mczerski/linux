/*
 * OR32 support for cache consistent memory.
 * Copyright (C) 2010 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2010 PetaLogix
 * Copyright (C) 2005 John Williams <jwilliams@itee.uq.edu.au>
 *
 * Based on Microblaze version which was derived from PowerPC version 
 * which was, in turn, derived from arch/arm/mm/consistent.c
 * Copyright (C) 2001 Dan Malek (dmalek@jlc.net)
 * Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/stddef.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/bootmem.h>
#include <linux/highmem.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/gfp.h>

#include <asm/pgalloc.h>
#include <linux/io.h>
#include <linux/hardirq.h>
#include <asm/mmu_context.h>
#include <asm/mmu.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>


static int map_page(unsigned long va, unsigned long pa, pgprot_t prot)
{
        pgd_t *pge;
        pud_t *pue;
        pmd_t *pme;
        pte_t *pte;
        int err = -ENOMEM;

        /* Use upper 10 bits of VA to index the first level map */
        pge = pgd_offset_k(va);
        pue = pud_offset(pge, va);
        pme = pmd_offset(pue, va);

        /* Use middle 10 bits of VA to index the second-level map */
        pte = pte_alloc_kernel(pme, va);
        if (pte != 0) {
                err = 0;
                set_pte(pte, mk_pte_phys(pa & PAGE_MASK, prot));
        }

        return err;
}

void *consistent_alloc(int gfp, size_t size, dma_addr_t *dma_handle)
{
	int order, err, i;
	unsigned long page, va;
	pgprot_t prot;
	phys_addr_t pa;
	struct vm_struct *area;
	void	 *ret;

	if (in_interrupt())
		BUG();

	/* Only allocate page size areas. */
	size = PAGE_ALIGN(size);
	order = get_order(size);

	page = __get_free_pages(gfp, order);
	if (!page) {
		BUG();
		return NULL;
	}

	/*
	 * we need to ensure that there are no cachelines in use,
	 * or worse dirty in this area.
	 */
//	flush_dcache_range(virt_to_phys(page), virt_to_phys(page) + size);

	/* Allocate some common virtual space to map the new pages. */
	area = get_vm_area(size, VM_ALLOC);
	if (area == NULL) {
		free_pages(page, order);
		return NULL;
	}
	va = (unsigned long) area->addr;
	ret = (void *)va;

	/* This gives us the real physical address of the first page. */
	*dma_handle = pa = virt_to_bus((void *)page);

	/* MS: This is the whole magic - use cache inhibit pages */
	prot = PAGE_KERNEL_NOCACHE;

	/*
	 * Set refcount=1 on all pages in an order>0
	 * allocation so that vfree() will actually
	 * free all pages that were allocated.
	 */
	if (order > 0) {
		struct page *rpage = virt_to_page(page);
		for (i = 1; i < (1 << order); i++)
			init_page_count(rpage+i);
	}

	err = 0;
	for (i = 0; i < size && err == 0; i += PAGE_SIZE)
		err = map_page(va+i, pa+i, prot);

	if (err) {
		vfree((void *)va);
		return NULL;
	}

	return ret;
}
EXPORT_SYMBOL(consistent_alloc);

/*
 * free page(s) as defined by the above mapping.
 */
void consistent_free(void *vaddr)
{
	if (in_interrupt())
		BUG();

	vfree(vaddr);
}
EXPORT_SYMBOL(consistent_free);

#if 0
/*
 * make an area consistent.
 */
void consistent_sync(void *vaddr, size_t size, int direction)
{
	unsigned long start;
	unsigned long end;

	start = (unsigned long)vaddr;

	end = start + size;

	switch (direction) {
	case PCI_DMA_NONE:
		BUG();
	case PCI_DMA_FROMDEVICE:	/* invalidate only */
		flush_dcache_range(start, end);
		break;
	case PCI_DMA_TODEVICE:		/* writeback only */
		flush_dcache_range(start, end);
		break;
	case PCI_DMA_BIDIRECTIONAL:	/* writeback and invalidate */
		flush_dcache_range(start, end);
		break;
	}
}
EXPORT_SYMBOL(consistent_sync);

/*
 * consistent_sync_page makes memory consistent. identical
 * to consistent_sync, but takes a struct page instead of a
 * virtual address
 */
void consistent_sync_page(struct page *page, unsigned long offset,
	size_t size, int direction)
{
	unsigned long start = (unsigned long)page_address(page) + offset;
	consistent_sync((void *)start, size, direction);
}
EXPORT_SYMBOL(consistent_sync_page);
#endif
