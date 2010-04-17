#ifdef __KERNEL__
#ifndef _OR32_SERIAL_H__
#define _OR32_SERIAL_H__

#include <asm/board.h>

/* JPB: Linux will set the default UART baud rate (for us this seems to be 9600)
 * based on the system clock. If you want a different baud rate, spoof the
 * system by giving a smaller value for BASE_BAUD, which Linux will use as the
 * system clock when computing baud rate.
 *
 * Original code placed an unwarranted divide by 16, thereby trying to set up
 * a 2 Mbit terminal!
 *
 * #define BASE_BAUD ( (SYS_CLK/(OR32_CONSOLE_BAUD)) * (9600/16) ) 
 */
#define BASE_BAUD ( (SYS_CLK/(OR32_CONSOLE_BAUD)) * (9600) )
 
#endif /* __ASM_SERIAL_H__ */
#endif /* __KERNEL__ */
