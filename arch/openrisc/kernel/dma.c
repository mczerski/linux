/*
 * OpenRISC Linux
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
 *
 * DMA mapping callbacks...
 * As alloc_coherent is the only DMA callback being used currently, that's
 * the only thing implemented properly.  The rest need looking into...
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/gfp.h>
#include <linux/dma-debug.h>
#include <linux/io.h>
#include <linux/vmalloc.h>

/*
 * Alloc "coherent" memory, which for OpenRISC means simply uncached.
 */
void *or1k_dma_alloc_coherent(struct device *dev, size_t size,
			      dma_addr_t *dma_handle, gfp_t flag)
{
	int order;
	unsigned long page, va;
	pgprot_t prot;
	struct vm_struct *area;

	/* Only allocate page size areas. */
	size = PAGE_ALIGN(size);
	order = get_order(size);

	page = __get_free_pages(flag, order);
	if (!page)
		return NULL;

	/* Allocate some common virtual space to map the new pages. */
	area = get_vm_area(size, VM_ALLOC);
	if (area == NULL) {
		free_pages(page, order);
		return NULL;
	}
	va = (unsigned long)area->addr;

	/* This gives us the real physical address of the first page. */
	*dma_handle = __pa(page);

	prot = PAGE_KERNEL_NOCACHE;

	/* This isn't so much ioremap as just simply 'remap' */
	if (ioremap_page_range(va, va + size, *dma_handle, prot)) {
		vfree(area->addr);
		return NULL;
	}

	return (void *)va;
}

void or1k_dma_free_coherent(struct device *dev, size_t size, void *vaddr,
			    dma_addr_t dma_handle)
{
	vfree(vaddr);
}

/* Number of entries preallocated for DMA-API debugging */
#define PREALLOC_DMA_DEBUG_ENTRIES (1 << 16)

static int __init dma_init(void)
{
	dma_debug_init(PREALLOC_DMA_DEBUG_ENTRIES);

	return 0;
}

fs_initcall(dma_init);
