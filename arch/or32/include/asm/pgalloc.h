#ifndef _OR32_PGALLOC_H
#define _OR32_PGALLOC_H

#include <asm/page.h>
#include <linux/threads.h>
#include <linux/mm.h>

#if 1
#define pmd_populate_kernel(mm, pmd, pte) \
                 set_pmd(pmd, __pmd(_KERNPG_TABLE + __pa(pte)))
 
static inline void pmd_populate(struct mm_struct *mm, pmd_t *pmd, struct page *pte)
{
	set_pmd(pmd, __pmd(_KERNPG_TABLE +
			   ((unsigned long)page_to_pfn(pte) <<
			    (unsigned long) PAGE_SHIFT)));
}
#endif

#if 0
#define pmd_populate_kernel(mm, pmd, pte) \
                 set_pmd(pmd, __pmd(_KERNPG_TABLE + __pa(pte)))
#define pmd_populate(mm, pmd, pte) \
                set_pmd(pmd, __pmd(__pa(pte)))

#endif

#if 0
/* __PHX__ check */
#define pmd_populate_kernel(mm, pmd, pte) pmd_set(pmd, pte)
#define pmd_populate(mm, pmd, pte) pmd_set(pmd, page_address(pte))
#endif 


#if 1
/*
 * Allocate and free page tables.
 */

static inline pgd_t *pgd_alloc(struct mm_struct *mm)
{  
	pgd_t *ret = (pgd_t *)__get_free_page(GFP_KERNEL);
	
	if (ret) {
		memset(ret, 0, USER_PTRS_PER_PGD * sizeof(pgd_t));
		memcpy(ret + USER_PTRS_PER_PGD, swapper_pg_dir + USER_PTRS_PER_PGD,
		       (PTRS_PER_PGD - USER_PTRS_PER_PGD) * sizeof(pgd_t));

	}
	return ret;
}
#endif

#if 0
/* __PHX__ check, this is supposed to be 2.6 style, but 
 * we use current_pgd (from mm->pgd) to load kernel pages
 * so we need it initialized.
 */
extern inline pgd_t *pgd_alloc (struct mm_struct *mm)
{
	return (pgd_t *)get_zeroed_page(GFP_KERNEL);
}
#endif

static inline void pgd_free (struct mm_struct *mm, pgd_t *pgd)
{
	free_page((unsigned long)pgd);
}

static inline pte_t *pte_alloc_one_kernel(struct mm_struct *mm, unsigned long address)
{
  	pte_t *pte = (pte_t *)__get_free_page(GFP_KERNEL|__GFP_REPEAT);
	if (pte)
		clear_page(pte);
 	return pte;
}

static inline struct page *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	struct page *pte;
	pte = alloc_pages(GFP_KERNEL|__GFP_REPEAT, 0);
	if (pte)
		clear_page(page_address(pte));
	return pte;
}

static inline void pte_free_kernel(struct mm_struct *mm, pte_t *pte)
{
	free_page((unsigned long)pte);
}

static inline void pte_free(struct mm_struct *mm, struct page *pte)
{
	__free_page(pte);
}


#define __pte_free_tlb(tlb,pte,addr) tlb_remove_page((tlb),(pte))
#define pmd_pgtable(pmd) pmd_page(pmd)

#define check_pgt_cache()          do { } while (0)

#endif
