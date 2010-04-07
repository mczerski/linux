#ifndef _ASM_OR32_UCONTEXT_H
#define _ASM_OR32_UCONTEXT_H

#include <asm/sigcontext.h>

struct ucontext {
	unsigned long	  uc_flags;
	struct ucontext  *uc_link;
	stack_t		  uc_stack;
	struct sigcontext uc_mcontext;
	sigset_t	  uc_sigmask;	/* mask last for extensibility */
};

#endif /* !_ASM_OR32_UCONTEXT_H */
