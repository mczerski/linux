#define HAVE_FF1 1

#ifdef HAVE_FF1

static inline int ffs(int x) {
	int ret;

	__asm__ ( "l.ff1 %0,%1"
		: "=r" (ret)
		: "r" (x));

	return ret;
}

#else
#include <asm-generic/bitops/ffs.h>
#endif

