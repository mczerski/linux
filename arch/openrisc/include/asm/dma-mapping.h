/*
 * OpenRISC Linux
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source
 * declaration.
 *
 * OpenRISC implementation:
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __ASM_OPENRISC_DMA_MAPPING_H
#define __ASM_OPENRISC_DMA_MAPPING_H

/*
 * See Documentation/PCI/PCI-DMA-mapping.txt and
 * Documentation/DMA-API.txt for documentation.
 */

#include <linux/dma-debug.h>
#include <asm-generic/dma-coherent.h>

#define DMA_ERROR_CODE		(~(dma_addr_t)0x0)

int dma_mapping_error(struct device *dev, dma_addr_t dma_addr);

int dma_supported(struct device *dev, u64 mask);

int dma_set_mask(struct device *dev, u64 mask);

#define dma_alloc_noncoherent(d, s, h, f) dma_alloc_coherent(d, s, h, f)
#define dma_free_noncoherent(d, s, v, h) dma_free_coherent(d, s, v, h)

void *or1k_dma_alloc_coherent(struct device *dev, size_t size,
			      dma_addr_t *dma_handle, gfp_t flag);
void or1k_dma_free_coherent(struct device *dev, size_t size, void *vaddr,
			    dma_addr_t dma_handle);

static inline void *dma_alloc_coherent(struct device *dev, size_t size,
					dma_addr_t *dma_handle, gfp_t flag)
{
	void *memory;

	memory = or1k_dma_alloc_coherent(dev, size, dma_handle, flag);

	debug_dma_alloc_coherent(dev, size, *dma_handle, memory);
	return memory;
}

static inline void dma_free_coherent(struct device *dev, size_t size,
				     void *cpu_addr, dma_addr_t dma_handle)
{
	debug_dma_free_coherent(dev, size, cpu_addr, dma_handle);
	or1k_dma_free_coherent(dev, size, cpu_addr, dma_handle);
}

#endif	/* __ASM_OPENRISC_DMA_MAPPING_H */
