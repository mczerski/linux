/*
 * OpenRISC Linux
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the original source
 * declaration.
 *
 * OpenRISC implementation:
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 * et al.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __ASM_OPENRISC_IO_H
#define __ASM_OPENRISC_IO_H

#include <asm/page.h>   /* for __va, __pa */
#include <asm/byteorder.h>


/*
 * Change virtual addresses to physical addresses and vice versa.
 */

static inline unsigned long
virt_to_phys(volatile void * address)
{
	return __pa(address);
}

static inline void*
phys_to_virt(unsigned long address)
{
	return __va(address);
}

extern void __iomem*
__ioremap(phys_addr_t offset, unsigned long size, unsigned long flags);

static inline void __iomem*
ioremap(phys_addr_t offset, unsigned long size)
{
	return __ioremap(offset, size, 0);
}

/* #define _PAGE_CI       0x002 */
static inline void __iomem*
ioremap_nocache(phys_addr_t offset, unsigned long size)
{
	return __ioremap(offset, size, 0x002);
}

extern void iounmap(void *addr);

//#define page_to_phys(page)	((page - mem_map) << PAGE_SHIFT)

/*
 * IO bus memory addresses are also 1:1 with the physical address
 */

/* Deprecated */
#define virt_to_bus virt_to_phys
#define bus_to_virt phys_to_virt

#define __raw_readb(addr) (*(volatile unsigned char *) (addr))
#define __raw_readw(addr) (*(volatile unsigned short *) (addr))
#define __raw_readl(addr) (*(volatile unsigned int *) (addr))

#define __raw_writeb(b,addr) ((*(volatile unsigned char *) (addr)) = (b))
#define __raw_writew(b,addr) ((*(volatile unsigned short *) (addr)) = (b))
#define __raw_writel(b,addr) ((*(volatile unsigned int *) (addr)) = (b))

/* Wishbone Interface
 *
 * The Wishbone bus can be both big or little-endian, but is generally
 * of the same endianess as the CPU ("native endian").  As peripherals
 * are generally synthesized together with the CPU, they will also be
 * of the same endianess.  In order to simplify things, we assume for
 * now that there are no memory-mapped IO devices on any other bus than
 * then the local Wishbone bus and that these devices are all native
 * endian.
 */

#define wb_ioread8(p)  __raw_readb(p)
#define wb_ioread16(p) __raw_readw(p)
#define wb_ioread32(p) __raw_readl(p)

#define wb_iowrite8(v,p)  __raw_writeb(v,p)
#define wb_iowrite16(v,p) __raw_writew(v,p)
#define wb_iowrite32(v,p) __raw_writel(v,p)

/*
 * readX/writeX() are used to access memory mapped devices.
 *
 * Note that these accessors make assumptions about the endianess of
 * the accessed device and, as such, aren't particularly useful on a
 * platform like OpenRISC where the device endianess is generally
 * determined at synthesis time and where the device endianess is
 * generally the same as the CPU (native endian).
 *
 * For OpenRISC it is recommended to use the __iomem accessors
 * instead of these MMIO accessors.
 */

#define readb __raw_readb
#define readw(addr) (le16_to_cpu(__raw_readw(addr)))
#define readl(addr) (le32_to_cpu(__raw_readl(addr)))

#define writeb __raw_writeb
#define writew(b,addr) __raw_writew(cpu_to_le16(b), addr)
#define writel(b,addr) __raw_writel(cpu_to_le32(b), addr)

#define memset_io(a,b,c)	memset((void *)(a),(b),(c))
#define memcpy_fromio(a,b,c)	memcpy((a),(void *)(b),(c))
#define memcpy_toio(a,b,c)	memcpy((void *)(a),(b),(c))

/*
 * Again, OpenRISC does not require mem IO specific function.
 */

#define eth_io_copy_and_sum(a,b,c,d)	eth_copy_and_sum((a),(void *)(b),(c),(d))

#define IO_BASE			0x0
#define IO_SPACE_LIMIT 		0xffffffff

#define inb(port)		(*(volatile unsigned char *) (port+IO_BASE))
#define outb(value,port)	((*(volatile unsigned char *) (port+IO_BASE)) = (value))	

#define inb_p(port)             inb((port))
#define outb_p(val, port)       outb((val), (port))

static inline void ioread8_rep(void __iomem *port, void *buf, unsigned long count)
{
	unsigned char *p = buf;
	while (count--)
		*p++ = readb(port);
}

static inline void ioread16_rep(void __iomem *port, void *buf, unsigned long count)
{
	unsigned short *p = buf;
	while (count >= 2) {
		*p++ = readw(port);
		count -= 2;
	}
}

static inline void ioread32_rep(void __iomem *port, void *buf, unsigned long count)
{
	unsigned int *p = buf;
	while (count >= 4) {
		*p++ = readl(port);
		count -= 4;
	}
}

static inline void iowrite8_rep(void __iomem *port, void *buf, unsigned long count)
{
	unsigned char *p = buf;
	while (count--)
		writeb(*p++, port);
}

static inline void iowrite16_rep(void __iomem *port, void *buf, unsigned long count)
{
	unsigned short *p = buf;
	while (count >= 2) {
		writew(*p++, port);
		count -= 2;
	}
}

static inline void iowrite32_rep(void __iomem *port, void *buf, unsigned long count)
{
	unsigned int *p = buf;
	while (count >= 4) {
		writel(*p++, port);
		count -= 4;
	}
}

/*
 * Convert a physical pointer to a virtual kernel pointer for /dev/mem
 * access
 */
#define xlate_dev_mem_ptr(p)	__va(p)

/*
 * Convert a virtual cached pointer to an uncached pointer
 */
#define xlate_dev_kmem_ptr(p)	p


/* Create a virtual mapping cookie for an IO port range */
extern void __iomem *ioport_map(unsigned long port, unsigned int nr);
extern void ioport_unmap(void __iomem *);

/* __iomem accessors
 *
 * These accessors work on __iomem cookies and the recommended means of
 * doing MMIO access for OpenRISC.  The current assumption for OpenRISC
 * is that the Wishbone bus is the only bus with memory mapped peripherals
 * and that the bus endianess (and device endianess) is the same as that
 * of the CPU.
 */

#define ioread8(addr)           wb_ioread8(addr)
#define ioread16(addr)          wb_ioread16(addr)
#define ioread32(addr)          wb_ioread32(addr)

#define iowrite8(v, addr)       wb_iowrite8((v),(addr))
#define iowrite16(v, addr)      wb_iowrite16((v),(addr))
#define iowrite32(v, addr)      wb_iowrite32((v),(addr))

#endif
