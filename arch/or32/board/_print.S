#include <asm/spr_defs.h>

 	.global __print
__print:
	l.lwz	r3,0(r1)
	l.addi	r4,r1,4
        l.nop 3
	l.jr	r9
	l.nop



