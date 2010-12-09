#if !defined(__ASM_OPENRISC_UNISTD_H) || defined(__SYSCALL)
#define __ASM_OPENRISC_UNISTD_H

#define __ARCH_HAVE_MMU

#define __ARCH_WANT_SYSCALL_NO_AT
#define __ARCH_WANT_SYSCALL_NO_FLAGS
#define __ARCH_WANT_SYSCALL_OFF_T
#define __ARCH_WANT_SYSCALL_DEPRECATED

#include <asm-generic/unistd.h>

#define __NR_or1k_atomic __NR_arch_specific_syscall
__SYSCALL(__NR_or1k_atomic, sys_or1k_atomic)

#endif /* __ASM_OPENRISC_UNISTD_H */
