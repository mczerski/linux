#ifndef __ASM_OPENRISC_SYSTEM_H
#define __ASM_OPENRISC_SYSTEM_H

#include <asm-generic/system.h>

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#define mtspr(_spr, _val) __asm__ __volatile__ ( 		\
	"l.mtspr r0,%1,%0"					\
	:: "K" (_spr), "r" (_val))
#define mtspr_off(_spr, _off, _val) __asm__ __volatile__ ( 	\
	"l.mtspr %0,%1,%2"					\
	:: "r" (_off), "r" (_val), "K" (_spr))

static inline unsigned long mfspr(unsigned long add)
{
	unsigned long ret;
	__asm__ __volatile__ ("l.mfspr %0,r0,%1" : "=r" (ret) : "K" (add));
	return ret;
}

/* We probably need this definition, but the generic system.h provides it
 * and it's not used on our arch anyway...
 */
/*#define nop() __asm__ __volatile__ ("l.nop"::)*/

#endif /* __ASSEMBLY__ */
#endif /* __KERNEL__ */
#endif /* __ASM_OPENRISC_SYSTEM_H */

