#ifndef __ASM_OPENRISC_IO_H
#define __ASM_OPENRISC_IO_H

#include <asm/page.h>   /* for __va, __pa */
#include <asm/byteorder.h>


/*
 * Change virtual addresses to physical addresses and vv.
 */

static inline unsigned long virt_to_phys(volatile void * address)
{
	return __pa(address);
}

static inline void * phys_to_virt(unsigned long address)
{
	return __va(address);
}

extern void * __ioremap(unsigned long offset, unsigned long size, unsigned long flags);

extern inline void * ioremap(unsigned long offset, unsigned long size)
{
	return __ioremap(offset, size, 0);
}

/* #define _PAGE_CI       0x002 */
extern inline void * ioremap_nocache(unsigned long offset, unsigned long size)
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

/*
 * readX/writeX() are used to access memory mapped devices. On some
 * architectures the memory mapped IO stuff needs to be accessed
 * differently. On the openrisc architecture, we just read/write the
 * memory location directly.
 */

#define __raw_readb(addr) (*(volatile unsigned char *) (addr))
#define __raw_readw(addr) (*(volatile unsigned short *) (addr))
#define __raw_readl(addr) (*(volatile unsigned int *) (addr))
#define readb __raw_readb
#define readw(addr) (le16_to_cpu(__raw_readw(addr)))
#define readl(addr) (le32_to_cpu(__raw_readl(addr)))

#define __raw_writeb(b,addr) ((*(volatile unsigned char *) (addr)) = (b))
#define __raw_writew(b,addr) ((*(volatile unsigned short *) (addr)) = (b))
#define __raw_writel(b,addr) ((*(volatile unsigned int *) (addr)) = (b))
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

/* ioread/iowrite are defined with the __raw_* variants here because we
 * haven't gotten the device/bus endianess bits straightened out for 
 * "native" endianess yet... these should be changed to the non-raw
 * variants when that work's done
 */

#define ioread8(addr)           readb(addr)
#define ioread16(addr)          __raw_readw(addr)
#define ioread32(addr)          __raw_readl(addr)

#define iowrite8(v, addr)       writeb((v), (addr))
#define iowrite16(v, addr)      __raw_writew((v), (addr))
#define iowrite32(v, addr)      __raw_writel((v), (addr))


#endif
