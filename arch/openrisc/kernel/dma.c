/*
 * Copyright (C) 2010 ORSoC
 * Copyright (C) 2009-2010 PetaLogix
 * Copyright (C) 2006 Benjamin Herrenschmidt, IBM Corporation
 *
 * Derived from Microblaze version.
 * Provide default implementations of the DMA mapping callbacks for
 * directly mapped busses.
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/gfp.h>
#include <linux/dma-debug.h>
#include <asm/bug.h>
#include <asm/cacheflush.h>

/*
 * Generic direct DMA implementation.
 *
 */
static inline void __dma_sync_page(unsigned long paddr, unsigned long offset,
				size_t size, enum dma_data_direction direction)
{
	switch (direction) {
	case DMA_TO_DEVICE:
//		flush_dcache_range(paddr + offset, paddr + offset + size);
		break;
	case DMA_FROM_DEVICE:
//		invalidate_dcache_range(paddr + offset, paddr + offset + size);
		break;
	default:
		BUG();
	}
}

static void *dma_direct_alloc_coherent(struct device *dev, size_t size,
				dma_addr_t *dma_handle, gfp_t flag)
{
	return consistent_alloc(flag, size, dma_handle);
}

static void dma_direct_free_coherent(struct device *dev, size_t size,
			      void *vaddr, dma_addr_t dma_handle)
{
	consistent_free(vaddr);
}

static int dma_direct_map_sg(struct device *dev, struct scatterlist *sgl,
			     int nents, enum dma_data_direction direction,
			     struct dma_attrs *attrs)
{
	struct scatterlist *sg;
	int i;

	/* FIXME this part of code is untested */
	for_each_sg(sgl, sg, nents, i) {
		sg->dma_address = sg_phys(sg);
		__dma_sync_page(page_to_phys(sg_page(sg)), sg->offset,
							sg->length, direction);
	}

	return nents;
}

static void dma_direct_unmap_sg(struct device *dev, struct scatterlist *sg,
				int nents, enum dma_data_direction direction,
				struct dma_attrs *attrs)
{
}

static int dma_direct_dma_supported(struct device *dev, u64 mask)
{
	return 1;
}

static inline dma_addr_t dma_direct_map_page(struct device *dev,
					     struct page *page,
					     unsigned long offset,
					     size_t size,
					     enum dma_data_direction direction,
					     struct dma_attrs *attrs)
{
	__dma_sync_page(page_to_phys(page), offset, size, direction);
	return page_to_phys(page) + offset;
}

static inline void dma_direct_unmap_page(struct device *dev,
					 dma_addr_t dma_address,
					 size_t size,
					 enum dma_data_direction direction,
					 struct dma_attrs *attrs)
{
	__dma_sync_page(dma_address, 0 , size, direction);
}

struct dma_map_ops dma_direct_ops = {
	.alloc_coherent	= dma_direct_alloc_coherent,
	.free_coherent	= dma_direct_free_coherent,
	.map_sg		= dma_direct_map_sg,
	.unmap_sg	= dma_direct_unmap_sg,
	.dma_supported	= dma_direct_dma_supported,
	.map_page	= dma_direct_map_page,
	.unmap_page	= dma_direct_unmap_page,
};
EXPORT_SYMBOL(dma_direct_ops);

/* Number of entries preallocated for DMA-API debugging */
#define PREALLOC_DMA_DEBUG_ENTRIES (1 << 16)

static int __init dma_init(void)
{
       dma_debug_init(PREALLOC_DMA_DEBUG_ENTRIES);

       return 0;
}
fs_initcall(dma_init);
