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
#include <asm/io.h>
#include <asm/pgalloc.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
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

/* bt ioremaped lenghts */
static unsigned int bt_ioremapped_len[NR_FIX_BTMAPS] __initdata = 
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

extern inline void remap_area_pte(pte_t * pte, unsigned long address, unsigned long size,
	unsigned long phys_addr, unsigned long flags)
{
	unsigned long end;

	address &= ~PMD_MASK;
	end = address + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;
	if (address >= end)
		BUG();
	do {
		if (!pte_none(*pte)) {
			printk("remap_area_pte: page already exists\n");
			BUG();
		}
		set_pte(pte, mk_pte_phys(phys_addr, __pgprot(_PAGE_PRESENT | __READABLE | 
							     __WRITEABLE | _PAGE_GLOBAL |
							     _PAGE_KERNEL | flags)));
		address += PAGE_SIZE;
		phys_addr += PAGE_SIZE;
		pte++;
	} while (address && (address < end));
}

static inline int remap_area_pmd(pmd_t * pmd, unsigned long address, unsigned long size,
	unsigned long phys_addr, unsigned long flags)
{
	unsigned long end;

	address &= ~PGDIR_MASK;
	end = address + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;
	phys_addr -= address;
	if (address >= end)
		BUG();
	do {
		pte_t * pte = pte_alloc_kernel(pmd, address);
		if (!pte)
			return -ENOMEM;
		remap_area_pte(pte, address, end - address, address + phys_addr, flags);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address && (address < end));
	return 0;
}

static int remap_area_pages(unsigned long address, unsigned long phys_addr,
				 unsigned long size, unsigned long flags)
{
	int error;
	pgd_t * dir;
	unsigned long end = address + size;

	phys_addr -= address;
	dir = pgd_offset(&init_mm, address);
	flush_cache_all();
	if (address >= end)
		BUG();
	spin_lock(&init_mm.page_table_lock);
	do {
		pmd_t *pmd;
		pmd = pmd_alloc(&init_mm, dir, address);
		error = -ENOMEM;
		if (!pmd)
			break;
		if (remap_area_pmd(pmd, address, end - address,
				   phys_addr + address, flags))
			break;
		error = 0;
		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		dir++;
	} while (address && (address < end));
	spin_unlock(&init_mm.page_table_lock);
	flush_tlb_all();
	return error;
}

/*
 * IO remapping core to use when system is running
 */
void *ioremap_core(unsigned long phys_addr, unsigned long size,
                   unsigned long flags)
{
	struct vm_struct * area;
	void * addr;

	area = get_vm_area(size, VM_IOREMAP);
	if (!area)
		return NULL;
	addr = area->addr;
	if (remap_area_pages((unsigned long) addr, phys_addr, size, flags)) {
		vfree(addr);
		return NULL;
	}
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
	if(mem_init_done)
		addr = ioremap_core(phys_addr, size, flags);
	else
		addr = bt_ioremap_core(phys_addr, size, flags);

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
