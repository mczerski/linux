/*
 * OpenRISC dma.c
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
#include <asm/bug.h>

/*
 * Alloc "coherent" memory, which for OpenRISC means simply uncached.
 */
static void*
or1k_dma_alloc_coherent(struct device *dev, size_t size,
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
	if (!page) {
		return NULL;
	}

	/* Allocate some common virtual space to map the new pages. */
	area = get_vm_area(size, VM_ALLOC);
	if (area == NULL) {
		free_pages(page, order);
		return NULL;
	}
	va = (unsigned long) area->addr;

	/* This gives us the real physical address of the first page. */
	//*dma_handle = __pa(page);

	prot = PAGE_KERNEL_NOCACHE;

	/* This isn't so much ioremap as just simply 'remap' */
	if (ioremap_page_range(va, va + size, page, prot)) {
		vfree(area->addr);
		return NULL;
	}

	*dma_handle = page;
	return (void*) va;
}

static void
or1k_dma_free_coherent(struct device* dev, size_t size, void* vaddr,
                  dma_addr_t dma_handle)
{
	vfree(vaddr);
}

static int
or1k_dma_map_sg(struct device *dev, struct scatterlist *sgl,
                int nents, enum dma_data_direction direction,
                struct dma_attrs *attrs)
{
	struct scatterlist *sg;
	int i;

	BUG();
	return 0;
#if 0
	/* FIXME this part of code is untested */
	for_each_sg(sgl, sg, nents, i) {
		sg->dma_address = sg_phys(sg);
		__dma_sync_page(page_to_phys(sg_page(sg)), sg->offset,
							sg->length, direction);
	}

	return nents;
#endif
}

static inline dma_addr_t
or1k_dma_map_page(struct device *dev,
                    struct page *page,
                    unsigned long offset,
                    size_t size,
                    enum dma_data_direction direction,
                    struct dma_attrs *attrs)
{
	BUG();
	return NULL;
}

#if 0
static void
__dma_map_range(dma_addr_t dma_addr, size_t size)
{
	struct page *page = pfn_to_page(PFN_DOWN(dma_addr));
	size_t bytesleft = PAGE_SIZE - (dma_addr & (PAGE_SIZE - 1));

	while ((ssize_t)size > 0) {
		/* Flush the page. */

                homecache_flush_cache(page++, 0);

                /* Figure out if we need to continue on the next page. */
                size -= bytesleft;
                bytesleft = PAGE_SIZE;
        }
cacheflush(...);
}
#endif

struct dma_map_ops or1k_dma_ops = {
	.alloc_coherent	= or1k_dma_alloc_coherent,
	.free_coherent	= or1k_dma_free_coherent,
	.map_sg		= or1k_dma_map_sg,
	.map_page	= or1k_dma_map_page,
};
EXPORT_SYMBOL(or1k_dma_ops);

/* Number of entries preallocated for DMA-API debugging */
#define PREALLOC_DMA_DEBUG_ENTRIES (1 << 16)

static int __init dma_init(void)
{
       dma_debug_init(PREALLOC_DMA_DEBUG_ENTRIES);

       return 0;
}
fs_initcall(dma_init);
