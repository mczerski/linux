#ifndef _OR32_PAGE_H
#define _OR32_PAGE_H


/* PAGE_SHIFT determines the page size */
#define PAGE_SHIFT		13
#define PAGE_SIZE		(1UL << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE-1))
//#define PAGE_ALIGN(addr)	(((addr)+PAGE_SIZE-1)&PAGE_MASK)

#define PAGE_OFFSET		0xc0000000	
#define KERNELBASE  		PAGE_OFFSET

#ifdef __KERNEL__

#ifndef __ASSEMBLY__
#define clear_page(page)        memset((void *)(page), 0, PAGE_SIZE)
#define copy_page(to,from)      memcpy((void *)(to), (void *)(from), PAGE_SIZE)

#define clear_user_page(page, vaddr, pg)    clear_page(page)
#define copy_user_page(to, from, vaddr, pg) copy_page(to, from)

#define STRICT_MM_TYPECHECKS
#ifdef  STRICT_MM_TYPECHECKS
/*
 * These are used to make use of C type-checking..
 */
typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned long pmd; } pmd_t;
typedef struct { unsigned long pgd; } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;
typedef struct page *pgtable_t; //rgd

#define pte_val(x)	((x).pte)
#define pmd_val(x)	((x).pmd)
#define pgd_val(x)	((x).pgd)
#define pgprot_val(x)	((x).pgprot)

#define __pte(x)	((pte_t) { (x) } )
#define __pmd(x)	((pmd_t) { (x) } )
#define __pgd(x)	((pgd_t) { (x) } )
#define __pgprot(x)	((pgprot_t) { (x) } )

#else /* STRICT_MM_TYPECHECKS */
/*
 * .. while these make it easier on the compiler
 */
typedef unsigned long pte_t;
typedef unsigned long pmd_t;
typedef unsigned long pgd_t;
typedef unsigned long pgprot_t;

#define pte_val(x)	(x)
#define pmd_val(x)	(x)
#define pgd_val(x)	(x)
#define pgprot_val(x)	(x)

#define __pte(x)	(x)
#define __pmd(x)	(x)
#define __pgd(x)	(x)
#define __pgprot(x)	(x)

#endif /* STRICT_MM_TYPECHECKS */


/* Pure 2^n version of get_order */
static __inline__ int get_order(unsigned long size)
{
	int order;

	size = (size-1) >> (PAGE_SHIFT-1);
	order = -1;
	do {
		size >>= 1;
		order++;
	} while (size);
	return order;
}


/* macros to convert between really physical and virtual addresses
 */

#define __pa(x)                 ((unsigned long)(x) - PAGE_OFFSET)
#define __va(x)                 ((void *)((unsigned long)(x) + PAGE_OFFSET))

#define pfn_to_page(pfn)	(mem_map + (pfn))
#define page_to_pfn(page)	((unsigned long)((page) - mem_map))

#define virt_to_page(kaddr)	pfn_to_page(__pa(kaddr) >> PAGE_SHIFT)
#define page_to_virt(page)	__va(page_to_pfn(page) << PAGE_SHIFT)

#define pfn_valid(pfn)		((pfn) < max_mapnr)
#define virt_addr_valid(kaddr)	pfn_valid(__pa(kaddr) >> PAGE_SHIFT)

/* from linker script */

extern unsigned long dram_start, dram_end;

#endif /* __ASSEMBLY__ */

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#endif /* __KERNEL__ */

#endif /* _OR32_PAGE_H */

