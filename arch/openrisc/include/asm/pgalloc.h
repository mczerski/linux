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

#ifndef __ASM_OPENRISC_PGALLOC_H
#define __ASM_OPENRISC_PGALLOC_H

#include <asm/page.h>
#include <linux/threads.h>
#include <linux/mm.h>
#include <linux/memblock.h>
#include <linux/bootmem.h>

extern int mem_init_done;

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

/**
 * OK, this one's a bit tricky... ioremap can get called before memory is
 * initialized (early serial console does this) and will want to alloc a page
 * for its mapping.  No userspace pages will ever get allocated before memory
 * is initialized so this applies only to kernel pages.  In the event that
 * this is called before memory is initialized we allocate the page using
 * the memblock infrastructure.
 */

static inline pte_t *pte_alloc_one_kernel(struct mm_struct *mm, unsigned long address)
{
	pte_t* pte;

	if (likely(mem_init_done)) {
		pte = (pte_t *)__get_free_page(GFP_KERNEL|__GFP_REPEAT);
	} else {
		pte = (pte_t *) alloc_bootmem_low_pages(PAGE_SIZE);
//		pte = (pte_t *) __va(memblock_alloc(PAGE_SIZE, PAGE_SIZE));
	}

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
