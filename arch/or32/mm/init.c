/*
 *  linux/arch/or32/mm/init.c
 *
 *  or32 version
 *    author(s): Matjaz Breskvar (phoenix@bsemi.com)
 *
 *  For more information about OpenRISC processors, licensing and
 *  design services you may contact Beyond Semiconductor at
 *  sales@bsemi.com or visit website http://www.bsemi.com.
 *
 *  derived from cris, i386, m68k, ppc, sh ports.
 *
 *  changes:
 *  18. 11. 2003: Matjaz Breskvar (phoenix@bsemi.com)
 *    initial port to or32 architecture
 *
 *  22. 11. 2003: Matjaz Breskvar (phoenix@bsemi.com)
 *    cleanups, identical mapping for serial console
 */
 
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
#include <linux/smp.h>
#include <linux/bootmem.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/blkdev.h>          /* for initrd_* */
#include <linux/pagemap.h>
#include <linux/memblock.h>

#include <asm/system.h>
#include <asm/segment.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/tlb.h>
#include <asm/mmu_context.h>
#include <asm/or32-hf.h>
#include <asm/kmap_types.h>
#include <asm/fixmap.h>

int mem_init_done = 0;

DEFINE_PER_CPU(struct mmu_gather, mmu_gathers);

void show_mem(void)
{
   
	int i,free = 0,total = 0,cached = 0, reserved = 0, nonshared = 0;
	int shared = 0;
	
	printk("\nMem-info:\n");
	show_free_areas();
	printk("Free swap:       %6ldkB\n",nr_swap_pages<<(PAGE_SHIFT-10));
	i = max_mapnr;
	while (i-- > 0) 
	{
		total++;
		if (PageReserved(mem_map+i))
			reserved++;
		else if (PageSwapCache(mem_map+i))
			cached++;
		else if (!page_count(mem_map+i))
			free++;
		else if (page_count(mem_map+i) == 1)
			nonshared++;
		else
			shared += page_count(mem_map+i) - 1;
	}
	
	printk("%d pages of RAM\n",total);
	printk("%d free pages\n",free);
	printk("%d reserved pages\n",reserved);
	printk("%d pages nonshared\n",nonshared);
	printk("%d pages shared\n",shared);
	printk("%d pages swap cached\n",cached);
}

pte_t *kmap_pte;
pgprot_t kmap_prot;

#define kmap_get_fixmap_pte(vaddr)					\
	pte_offset_kernel(pmd_offset(pud_offset(pgd_offset_k(vaddr), (vaddr)), (vaddr)), (vaddr))

static void __init kmap_init(void)
{
	unsigned long kmap_vstart;

	/* cache the first kmap pte */
	kmap_vstart = __fix_to_virt(FIX_KMAP_BEGIN);
	kmap_pte = kmap_get_fixmap_pte(kmap_vstart);

	kmap_prot = PAGE_KERNEL;
}

static void __init fixrange_init (unsigned long start, unsigned long end, pgd_t *pgd_base)
{
        pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;
	int i, j;
	unsigned long vaddr;
	
	vaddr = start;
	i = __pgd_offset(vaddr);
	j = __pmd_offset(vaddr);
	pgd = pgd_base + i;
	
	for ( ; (i < PTRS_PER_PGD) && (vaddr != end); pgd++, i++) {
		pmd = (pmd_t *)pgd;
		
		for (; (j < PTRS_PER_PMD) && (vaddr != end); pmd++, j++) {
			if (pmd_none(*pmd)) {
				pte = (pte_t *) alloc_bootmem_low_pages(PAGE_SIZE);
				set_pmd(pmd, __pmd(_KERNPG_TABLE + __pa(pte)));
				if (pte != pte_offset_kernel(pmd, 0))
					BUG();
			}
			vaddr += PMD_SIZE;
		}
		j = 0;
	}
}

static void __init zone_sizes_init(void)
{
	unsigned long zones_size[MAX_NR_ZONES];

	/* Clear the zone sizes */
	memset(zones_size, 0, sizeof(zones_size)); 
 
	/*
	 * We use only ZONE_NORMAL 
	*/ 
	zones_size[ZONE_NORMAL] = max_low_pfn; 

	free_area_init(zones_size);
}

static int map_page(unsigned long va, unsigned long pa, pgprot_t prot)
{
        pgd_t *pge;
        pud_t *pue;
        pmd_t *pme;
        pte_t *pte;
        int err = -ENOMEM;

        /* Use upper 8 bits of VA to index the first level map and
	 * next 11 bits of VA to index the second-level map.  This is
	 * hidden in the details of the following functions. */
        pge = pgd_offset_k(va);
        pue = pud_offset(pge, va);
        pme = pmd_offset(pue, va);

//        pte = pte_alloc_kernel(pme, va);
	pte = (pte_t *) alloc_bootmem_low_pages(PAGE_SIZE);
        if (pte != 0) {
                err = 0;
                set_pte(pte, mk_pte_phys(pa & PAGE_MASK, prot));
        }

        return err;
}

/*
 * Map all physical memory into kernel's address space.
 *
 * This is explicitly coded for two-level page tables, so if you need
 * something else then this needs to change.
 */
static void __init map_ram(void)
{
        unsigned long v, p, s, e;
	pgprot_t prot;
	int i;
        pgd_t *pge;
        pud_t *pue;
        pmd_t *pme;
        pte_t *pte;
	/* These mark extents of read-only kernel pages...
	 * ...from vmlinux.lds.S
 	 */
	extern const unsigned long _s_kernel_ro, _e_kernel_ro;

	v = PAGE_OFFSET;

	for (i = 0; i < memblock.memory.cnt; i++) {
		p = (u32) memblock.memory.region[i].base & PAGE_MASK;
		e = p + (u32) memblock.memory.region[i].size;

		v = (u32) __va(p);
		pge = pgd_offset_k(v);

		while (p < e) {
			int j;
			pue = pud_offset(pge,v);
			pme = pmd_offset(pue,v);

			if (pue != pge || pme != pge) {
				panic("%s: OR1K kernel hardcoded for two-level page tables", __func__);
			}

			/* Alloc one page for holding PTE's... */
			pte = (pte_t*) alloc_bootmem_low_pages(PAGE_SIZE);
			set_pmd(pme, __pmd(_KERNPG_TABLE + __pa(pte)));

			/* Fill the newly allocated page with PTE'S */
			for (j = 0; p < e && j < PTRS_PER_PGD; 
			     v += PAGE_SIZE, p += PAGE_SIZE, j++, pte++) {
				if (v > _e_kernel_ro || 
				    v < ((_s_kernel_ro >> PAGE_SHIFT) << PAGE_SHIFT))
					prot = PAGE_KERNEL;
				else
					prot = PAGE_KERNEL_RO;

				set_pte(pte, mk_pte_phys(p, prot));
			}

			pge++;
		}

		printk(KERN_INFO "%s: Memory: 0x%x-0x%x\n", __func__,
			(u32) memblock.memory.region[i].base,
			(u32) memblock.memory.region[i].base + (u32) memblock.memory.region[i].size);
	}
}


void __init paging_init(void)
{
	extern void tlb_init(void);

	unsigned long vaddr, end;
	pgd_t *pgd, *pgd_base;
	int i, j, k;
	pmd_t *pmd;
	pte_t *pte, *pte_base;

	printk("Setting up paging and PTEs.\n");

	/* clear out the init_mm.pgd that will contain the kernel's mappings */

	for(i = 0; i < PTRS_PER_PGD; i++)
		swapper_pg_dir[i] = __pgd(0);
	
	/* make sure the current pgd table points to something sane
	 * (even if it is most probably not used until the next 
	 *  switch_mm)
	 */
         current_pgd = init_mm.pgd;
 
         /* initialise the TLB (tlb.c) */
         tlb_init();

	end = (unsigned long)__va(max_low_pfn*PAGE_SIZE);

	map_ram();

	zone_sizes_init();

	/*
	 * Fixed mappings, only the page table structure has to be
	 * created - mappings will be set by set_fixmap():
	 */
	vaddr = __fix_to_virt(__end_of_fixed_addresses - 1) & PMD_MASK;
	pgd_base = swapper_pg_dir;
	fixrange_init(vaddr, 0, pgd_base);

	/*
	 * enable EA translations via PT mechanism
	 */
	
	/* self modifing code ;) */
	{
	  extern unsigned long dtlb_miss_handler;
	  extern unsigned long itlb_miss_handler;

	  unsigned long *dtlb_vector = __va(0x900);
	  unsigned long *itlb_vector = __va(0xa00);

	  printk("dtlb_miss_handler %p\n", &dtlb_miss_handler);
	  *dtlb_vector = ((unsigned long)&dtlb_miss_handler - 
			  (unsigned long)dtlb_vector) >> 2;

  	  printk("itlb_miss_handler %p\n", &itlb_miss_handler);
	  *itlb_vector = ((unsigned long)&itlb_miss_handler - 
			  (unsigned long)itlb_vector) >> 2;
	}

	/* Invalidate instruction caches after code modification */
	mtspr(SPR_ICBIR, 0x900);
	mtspr(SPR_ICBIR, 0xa00);

	kmap_init();
}


/* References to section boundaries */

extern char _stext, _etext, _edata, __bss_start, _end;
extern char __init_begin, __init_end;

/*unsigned long loops_per_usec;  Removed by JPB*/

static int __init free_pages_init(void)
{
	int reservedpages, pfn;
	
	/* this will put all low memory onto the freelists */
	totalram_pages = free_all_bootmem();
	
	reservedpages = 0;
	for (pfn = 0; pfn < max_low_pfn; pfn++) {
		/*
		 * Only count reserved RAM pages
		 */
		if (PageReserved(mem_map+pfn))
			reservedpages++;
	}

	return reservedpages;
}

static void __init set_max_mapnr_init(void)
{
        max_mapnr = num_physpages = max_low_pfn;
}

void __init mem_init(void)
{
	int codesize, reservedpages, datasize, initsize;

	phx_warn("mem_map %p", mem_map);
	if (!mem_map)
		BUG();
	
	set_max_mapnr_init();

	high_memory = (void *) __va(max_low_pfn * PAGE_SIZE);

	/* clear the zero-page */
	phx_printk("empty_zero_page %p", empty_zero_page);
	memset((void*)empty_zero_page, 0, PAGE_SIZE);

	reservedpages = free_pages_init();
	
	codesize =  (unsigned long) &_etext - (unsigned long) &_stext;
	datasize =  (unsigned long) &_edata - (unsigned long) &_etext;
	initsize =  (unsigned long) &__init_end - (unsigned long) &__init_begin;

	printk(KERN_INFO "Memory: %luk/%luk available (%dk kernel code, %dk reserved, %dk data, %dk init, %ldk highmem)\n",
		(unsigned long) nr_free_pages() << (PAGE_SHIFT-10),
		max_mapnr << (PAGE_SHIFT-10),
		codesize >> 10,
		reservedpages << (PAGE_SHIFT-10),
		datasize >> 10,
		initsize >> 10,
		(unsigned long) (0 << (PAGE_SHIFT-10))
	       );

	printk("mem_init_done ...........................................\n");
	mem_init_done = 1;
	return;
}

#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{
	printk (KERN_INFO "Freeing initrd memory: %ldk freed\n", (end - start) >> 10);
	
	for (; start < end; start += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(start));
		init_page_count(virt_to_page(start));
		free_page(start);
		totalram_pages++;
	}
}
#endif

void free_initmem(void)
{
        unsigned long addr;
	
        addr = (unsigned long)(&__init_begin);
        for (; addr < (unsigned long)(&__init_end); addr += PAGE_SIZE) {
                ClearPageReserved(virt_to_page(addr));
                init_page_count(virt_to_page(addr));
                free_page(addr);
                totalram_pages++;
        }
        printk (KERN_INFO "Freeing unused kernel memory: %luk freed\n", 
		((unsigned long)&__init_end - (unsigned long)&__init_begin) >> 10);
}

/*
 * Associate a virtual page frame with a given physical page frame 
 * and protection flags for that frame.
 */ 
static void set_pte_pfn(unsigned long vaddr, unsigned long pfn, pgprot_t flags)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd = swapper_pg_dir + pgd_index(vaddr);
	if (pgd_none(*pgd)) {
		BUG();
		return;
	}
	pud = pud_offset(pgd, vaddr);
	if (pud_none(*pud)) {
		BUG();
		return;
	}
	pmd = pmd_offset(pud, vaddr);
	if (pmd_none(*pmd)) {
		BUG();
		return;
	}
	pte = pte_offset_kernel(pmd, vaddr);
	/* <pfn,flags> stored as-is, to permit clearing entries */
	set_pte(pte, pfn_pte(pfn, flags));
}

void __set_fixmap (enum fixed_addresses idx, unsigned long phys, pgprot_t flags)
{
	unsigned long address = __fix_to_virt(idx);

	if (idx >= __end_of_fixed_addresses) {
		BUG();
		return;
	}
	set_pte_pfn(address, phys >> PAGE_SHIFT, flags);
	flush_tlb_all();
}

