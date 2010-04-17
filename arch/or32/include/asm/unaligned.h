#ifdef __KERNEL__
#ifndef _OR32_UNALIGNED_H
#define _OR32_UNALIGNED_H

#include <linux/unaligned/be_struct.h>
#include <linux/unaligned/generic.h>

/*
 * Select endianness, or rather this SHOULD be selectable
 * for now, though, hardcoded to BIG endian - jb
 */
#define get_unaligned	__get_unaligned_be
#define put_unaligned	__put_unaligned_be

/*
#define get_unaligned(ptr) ({			\
	typeof((*(ptr))) x;			\
	memcpy(&x, (void*)ptr, sizeof(*(ptr)));	\
	x;					\
})

#define put_unaligned(val, ptr) ({		\
	typeof((*(ptr))) x = val;		\
	memcpy((void*)ptr, &x, sizeof(*(ptr)));	\
})
*/

#endif
#endif /* __KERNEL__ */
