#ifndef __ASM_OPENRISC_UNALIGNED_H
#define __ASM_OPENRISC_UNALIGNED_H

/*
 * This is copied from the generic implementation and the C-struct
 * variant replaced with the memmove variant.  The GCC compiler
 * for the OR32 arch optimizes too aggressively for the C-struct
 * variant to work, so use the memmove variant instead.
 *
 * It may be worth considering implementing the unaligned access
 * exception handler and allowing unaligned accesses (access_ok.h)... 
 * not sure if it would be much of a performance win without further
 * investigation.
 */
#include <asm/byteorder.h>

#if defined(__LITTLE_ENDIAN)
# include <linux/unaligned/le_memmove.h>
# include <linux/unaligned/be_byteshift.h>
# include <linux/unaligned/generic.h>
# define get_unaligned	__get_unaligned_le
# define put_unaligned	__put_unaligned_le
#elif defined(__BIG_ENDIAN)
# include <linux/unaligned/be_memmove.h>
# include <linux/unaligned/le_byteshift.h>
# include <linux/unaligned/generic.h>
# define get_unaligned	__get_unaligned_be
# define put_unaligned	__put_unaligned_be
#else
# error need to define endianess
#endif

#endif /* __ASM_OPENRISC_UNALIGNED_H */
