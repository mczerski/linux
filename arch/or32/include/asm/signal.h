#ifndef __ASM_OPENRISC_SIGNAL_H__
#define __ASM_OPENRISC_SIGNAL_H__

/* Both SA_RESTORER and old_sigaction are obsolete and
 * should be removed, but the code in arch/or32/kernel/signal.c
 * still depends on them... so leave them here until the 
 * signal code can be cleaned up.
 */

#define SA_RESTORER    0x04000000

#include <asm-generic/signal.h>

struct old_sigaction {
       __sighandler_t sa_handler;
       old_sigset_t sa_mask;
       unsigned long sa_flags;
       void (*sa_restorer)(void);
};

#endif /* __ASM_OPENRISC_SIGNAL_H__ */
