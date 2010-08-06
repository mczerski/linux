#ifndef __ASM_OPENRISC_STRING_H
#define __ASM_OPENRISC_STRING_H


/* __PHX_TODO__: these are optimizations, will do later */
#if 0

#define __HAVE_ARCH_MEMCPY
extern void *memcpy(void *, const void *, size_t);

/* New and improved.  In arch/cris/lib/memset.c */

#define __HAVE_ARCH_MEMSET
extern void *memset(void *, int, size_t);

#endif

#endif
