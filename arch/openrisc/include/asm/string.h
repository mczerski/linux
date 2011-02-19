#ifndef __ASM_OPENRISC_STRING_H
#define __ASM_OPENRISC_STRING_H

/* TODO: Implement optimized version of memcpy and memset */

#if 0

#define __HAVE_ARCH_MEMCPY
extern void *memcpy(void *, const void *, size_t);

#define __HAVE_ARCH_MEMSET
extern void *memset(void *, int, size_t);

#endif

#endif
