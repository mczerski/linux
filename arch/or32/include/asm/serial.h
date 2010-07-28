#ifdef __KERNEL__
#ifndef _OR32_SERIAL_H__
#define _OR32_SERIAL_H__

//#include <asm-generic/serial.h>

#include <asm/board.h>

/* There's a generic version of this file, but it assumes a 1.8MHz UART clk...
 * this, on the other hand, assumes the UART clock is tied to the system 
 * clock...
 */

#define BASE_BAUD (SYS_CLK/16)

#endif /* __ASM_SERIAL_H__ */
#endif /* __KERNEL__ */
