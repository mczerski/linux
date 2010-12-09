#ifndef __ASM_OPENRISC_SYSCALLS_H
#define __ASM_OPENRISC_SYSCALLS_H

asmlinkage long sys_or1k_atomic(unsigned long type, unsigned long* v1, unsigned long* v2);

#include <asm-generic/syscalls.h>

#endif /* __ASM_OPENRISC_SYSCALLS_H */
