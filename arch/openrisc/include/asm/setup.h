#ifndef __ASM_OPENRISC_SETUP_H
#define __ASM_OPENRISC_SETUP_H

#include <asm-generic/setup.h>

#ifndef __ASSEMBLY__
#ifdef __KERNEL__
extern char cmd_line[COMMAND_LINE_SIZE];
#endif
#endif

#endif /* __ASM_OPENRISC_SETUP_H */
