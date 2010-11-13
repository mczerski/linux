#ifndef __ASM_OPENRISC_DELAY_H
#define __ASM_OPENRISC_DELAY_H

#include <linux/linkage.h>
#include <asm/param.h>			/* Added by JPB: for HZ */


extern inline void __delay(int loops)
{
	__asm__ __volatile__ (
			      "l.srli %0,%0,1;"
			      "1: l.sfeqi %0,0;"
			      "l.bnf 1b;"
			      "l.addi %0,%0,-1;"
			      : "=r" (loops): "0" (loops));
}


/* Use only for very small delays ( < 1 msec).  */

extern unsigned long loops_per_jiffy;

extern inline void udelay(unsigned long usecs)
{
  /*	__delay(usecs * loops_per_usec);  Removed by JPB */
	__delay( usecs * loops_per_jiffy * HZ / 1000000 );	/* JPB */
}

#endif 



