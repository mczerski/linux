/*
 * OpenRISC ioremap.c
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source 
 * declaration.
 *
 * Modifications for the OpenRISC architecture:
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/vmalloc.h>
#include <linux/io.h>
#include <asm/pgalloc.h>
#include <asm/kmap_types.h>
#include <asm/fixmap.h>
#include <asm/bug.h>
#include <asm/pgtable.h>
#include <linux/sched.h>

extern int mem_init_done;

static unsigned int fixmaps_used __initdata = 0;

/*
 * Remap an arbitrary physical address space into the kernel virtual
 * address space. Needed when the kernel wants to access high addresses
 * directly.
 *
 * NOTE! We need to allow non-page-aligned mappings too: we will obviously
 * have to convert them into an offset in a page-aligned mapping, but the
 * caller shouldn't need to know that small detail.
 */
void __iomem* __init_refok
__ioremap(phys_addr_t addr, unsigned long size, unsigned long flags)
{
	phys_addr_t p;
	unsigned long v;
	unsigned long offset, last_addr;
	struct vm_struct *area = NULL;
	pgprot_t prot;

	/* Don't allow wraparound or zero size */
	last_addr = addr + size - 1;
	if (!size || last_addr < addr)
		return NULL;

	/*
	 * Mappings have to be page-aligned
	 */
	offset = addr & ~PAGE_MASK;
	p = addr & PAGE_MASK;
	size = PAGE_ALIGN(last_addr+1) - p;

	if (likely(mem_init_done)) {
		area = get_vm_area(size, VM_IOREMAP);
		if (!area)
			return NULL;
		v = (unsigned long) area->addr;
	} else {
		if ((fixmaps_used + (size >> PAGE_SHIFT)) > FIX_N_IOREMAPS)
			return NULL;
		v = fix_to_virt(FIX_IOREMAP_BEGIN+fixmaps_used);
		fixmaps_used += (size >> PAGE_SHIFT);
	}

	prot = __pgprot(_PAGE_ALL | _PAGE_SRE | _PAGE_SWE |
			_PAGE_SHARED | _PAGE_DIRTY | _PAGE_EXEC | flags);

	if (ioremap_page_range(v, v + size, p, prot)) {
		if (likely(mem_init_done))
			vfree(area->addr);
		else
			fixmaps_used -= (size >> PAGE_SHIFT);
		return NULL;
	}


	return (void __iomem *) (offset + (char *)v);
}

void iounmap(void *addr)
{
	/* If the page is from the fixmap pool then we just clear out
	 * the fixmap mapping.
	 */ 
	if (unlikely((unsigned long)addr > FIXADDR_START)) {
		clear_fixmap(virt_to_fix((unsigned long) addr));
		return;
	}
		
	return vfree((void *) (PAGE_MASK & (unsigned long) addr));
}

/*
 * OR1K has no port-mapped IO, only MMIO
 */
void __iomem *ioport_map(unsigned long port, unsigned int len)
{
	BUG();
}

void ioport_unmap(void __iomem *addr)
{
	BUG();	
}
