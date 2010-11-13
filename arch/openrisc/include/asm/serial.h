#ifndef __ASM_OPENRISC_SERIAL_H
#define __ASM_OPENRISC_SERIAL_H

#ifdef __KERNEL__

#include <asm/cpuinfo.h>

/* There's a generic version of this file, but it assumes a 1.8MHz UART clk...
 * this, on the other hand, assumes the UART clock is tied to the system 
 * clock... 8250_early.c (early 8250 serial console) actually uses this, so
 * it needs to be correct to get the early console working.
 */

#define BASE_BAUD (cpuinfo.clock_frequency/16)

#endif /* __KERNEL__ */

#endif /* __ASM_OPENRISC_SERIAL_H */
