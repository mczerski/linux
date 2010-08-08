#ifndef __ASM_OPENRISC_FLS_H
#define __ASM_OPENRISC_FLS_H


#ifdef CONFIG_OPENRISC_HAVE_INST_FL1

static inline int fls(int x) {
	int ret;

	__asm__ ( "l.fl1 %0,%1"
		: "=r" (ret)
		: "r" (x));

	return ret;
}

#else
#include <asm-generic/bitops/fls.h>
#endif

#endif /* __ASM_OPENRISC_FLS_H */
