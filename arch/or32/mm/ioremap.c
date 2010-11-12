/*
 * arch/or32/mm/ioremap.c
 *
 * Re-map IO memory to kernel address space so that we can access it.
 * Needed for memory-mapped I/O devices mapped outside our normal DRAM
 * window (that is, all memory-mapped I/O devices).
 *
 * (C) Copyright 1995 1996 Linus Torvalds
 * CRIS-port by Axis Communications AB
 */

#include <linux/vmalloc.h>
#include <linux/io.h>
//#include <asm/io.h>
#include <asm/pgalloc.h>
//#include <asm/cacheflush.h>
//#include <asm/tlbflush.h>
#include <asm/kmap_types.h>
#include <asm/fixmap.h>
#include <asm/bug.h>
#include <linux/sched.h>

/* __PHX__ cleanup, check */
#define __READABLE   ( _PAGE_ALL | _PAGE_URE | _PAGE_SRE )
#define __WRITEABLE  ( _PAGE_WRITE )
#define _PAGE_GLOBAL ( 0 )
#define _PAGE_KERNEL ( _PAGE_ALL | _PAGE_SRE | _PAGE_SWE | _PAGE_SHARED | _PAGE_DIRTY | _PAGE_EXEC )

extern int mem_init_done;

static unsigned long ioremap_bot = 0xffff0000L;

/* bt ioremaped lenghts */
static unsigned int bt_ioremapped_len[NR_FIX_BTMAPS] __initdata = 
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/*
 * IO remapping core to use when system is running
 */
void *ioremap_core(unsigned long phys_addr, unsigned long size,
                   unsigned long flags)
{
	struct vm_struct * area;
	unsigned long addr;
	pgprot_t prot;

	if (likely(mem_init_done)) {
		area = get_vm_area(size, VM_IOREMAP);
		if (!area)
			return NULL;
		addr = (unsigned long) area->addr;
	} else {
		ioremap_bot -= size;
		addr = ioremap_bot;
	}

	prot = __pgprot(_PAGE_PRESENT | __READABLE | __WRITEABLE | _PAGE_GLOBAL |
		        _PAGE_KERNEL | flags);

	if (ioremap_page_range((unsigned long) addr, addr + size, phys_addr, prot)) {
		if (mem_init_done)
			vfree(area->addr);
		else
			ioremap_bot += size;
		return NULL;
	}
/* Is this flush necessary??  Can we get flush_cache_vmap to cover it??? */
//	flush_tlb_all();
	return addr;
}

/*
 * Boot-time IO remapping core to use
 */
static void __init *bt_ioremap_core(unsigned long phys_addr, unsigned long size,
                                    unsigned long flags)
{
	unsigned int nrpages;
	unsigned int i;
	unsigned int nr_free;
	unsigned int idx;

	/*
	 * Mappings have to fit in the FIX_BTMAP area.
	 */
	nrpages = size >> PAGE_SHIFT;
	if (nrpages > NR_FIX_BTMAPS)
		return NULL;

	/*
	 * Find a big enough gap in NR_FIX_BTMAPS
	 */
	idx = FIX_BTMAP_BEGIN;
	for(i = 0, nr_free = 0; i < NR_FIX_BTMAPS; i++) {
		if(!bt_ioremapped_len[i])
			nr_free++;
		else {
			nr_free = 0;
			idx = FIX_BTMAP_BEGIN - i;
			i += bt_ioremapped_len[i] - 2;
		}
		if(nr_free == nrpages)
			break;
	}

	if(nr_free < nrpages)
		return NULL;

	bt_ioremapped_len[FIX_BTMAP_BEGIN - idx] = nrpages;

	/*
	 * Ok, go for it..
	 */
	for(i = idx; nrpages > 0; i--, nrpages--) {
		set_fixmap_nocache(i, phys_addr);
		phys_addr += PAGE_SIZE;
	}

	return (void *)fix_to_virt(idx);
}

static void __init bt_iounmap(void *addr)
{
	unsigned long virt_addr;
	unsigned int nr_pages;
	unsigned int idx;

	virt_addr = (unsigned long)addr;
	idx = virt_to_fix(virt_addr);
	nr_pages = bt_ioremapped_len[FIX_BTMAP_BEGIN - idx];
	bt_ioremapped_len[FIX_BTMAP_BEGIN - idx] = 0;

	while (nr_pages > 0) {
		clear_fixmap(idx);
		--idx;
		--nr_pages;
	}
}

/*
 * Generic mapping function (not visible outside):
 */

/*
 * Remap an arbitrary physical address space into the kernel virtual
 * address space. Needed when the kernel wants to access high addresses
 * directly.
 *
 * NOTE! We need to allow non-page-aligned mappings too: we will obviously
 * have to convert them into an offset in a page-aligned mapping, but the
 * caller shouldn't need to know that small detail.
 */
void * __ioremap(unsigned long phys_addr, unsigned long size, unsigned long flags)
{
	void * addr;
	unsigned long offset, last_addr;

	/* Don't allow wraparound or zero size */
	last_addr = phys_addr + size - 1;
	if (!size || last_addr < phys_addr)
		return NULL;

#if 0
	/* TODO: Here we can put checks for driver-writer abuse...  */

	/*
	 * Don't remap the low PCI/ISA area, it's always mapped..
	 */
	if (phys_addr >= 0xA0000 && last_addr < 0x100000)
		return phys_to_virt(phys_addr);

	/*
	 * Don't allow anybody to remap normal RAM that we're using..
	 */
	if (phys_addr < virt_to_phys(high_memory)) {
		char *t_addr, *t_end;
		struct page *page;

		t_addr = __va(phys_addr);
		t_end = t_addr + (size - 1);
	   
		for(page = virt_to_page(t_addr); page <= virt_to_page(t_end); page++)
			if(!PageReserved(page))
				return NULL;
	}
#endif

	/*
	 * Mappings have to be page-aligned
	 */
	offset = phys_addr & ~PAGE_MASK;
	phys_addr &= PAGE_MASK;
	size = PAGE_ALIGN(last_addr+1) - phys_addr;

	/*
	 * Ok, go for it..
	 */
	if(mem_init_done) {
		addr = ioremap_core(phys_addr, size, flags);
	} else {
		printk("JONAS JONAS: bt_iorempa_core\n");
		addr = ioremap_core(phys_addr, size, flags);
		//addr = bt_ioremap_core(phys_addr, size, flags);
	}

	return (void *) (offset + (char *)addr);
}

static inline int is_bt_ioremapped(void *addr)
{
	unsigned long a = (unsigned long)addr;
	return (a < FIXADDR_TOP) && (a >= FIXADDR_BOOT_START);
}

void iounmap(void *addr)
{
	if(is_bt_ioremapped(addr))
		return bt_iounmap(addr);
	if (addr > high_memory)
		return vfree((void *) (PAGE_MASK & (unsigned long) addr));
}


//RGD stolen from PPC probably doesn't work on or32 not called right now
void __iomem *ioport_map(unsigned long port, unsigned int len)
{
	return (void __iomem *) (port + IO_BASE);
}

void ioport_unmap(void __iomem *addr)
{
	/* Nothing to do */
}
