	.global _su_asm
_su_asm:
	l.addi   r1,r1,-8
	l.sw     0x0(r1),r9
        l.sw     0x4(r1),r14

	/* r14 = 0x8890 */
	l.ori	 r11,r0,0x0
	l.ori    r14,r0,0x8890
	l.nop
	l.nop
	
	l.sfgtui r14,0x8887
	l.bf     1f
	l.nop
	l.ori	 r11,r11,0x1    /* this should pass */

1:
	l.sfgtsi r14,0x8887     /* IMMEDIATE is NEGATIVE */
	l.bf     1f
	l.nop
	l.ori	 r11,r11,0x2    /* this should pass */

1:	
	l.sfgtui r14,0x6665
	l.bf     1f
	l.nop
	l.ori	 r11,r11,0x4    /* this should pass */

1:
	l.sfgtsi r14,0x6665
	l.bf     1f
	l.nop
	l.ori	 r11,r11,0x8    /* this should pass */


1:	
	/* r14 = 0x8008 */	
	l.ori	 r14,r0,0x8008	

	l.sfgtui r14,0x8887
	l.bf     1f
	l.nop
	l.ori	 r11,r11,0x10   /* this should fail r14 |= 0x10 */	
	
1:		
	l.sfgtsi r14,0x8887     /* IMMEDIATE is NEGATIVE */
	l.bf     1f
	l.nop
	l.ori	 r11,r11,0x20   /* this should pass  */

1:	

	/* r14 = 1 */	
	l.ori 	 r14,r0,1	

	l.sfgtsi r14,0x8887     /* IMMEDIATE is NEGATIVE */
	l.bf     1f
	l.nop
	l.ori	 r11,r11,0x40   /* this should pass */
	

1:
	l.sfgtui r14,0x8887
	l.bf     1f
	l.nop
	l.ori	 r11,r11,0x80   /* this should fail r14 |= 0x80 */

1:	
	l.sfgtsi r14,0x6665
	l.bf     1f
	l.nop
	l.ori	 r11,r11,0x100  /* this should fail r14 |= 0x100 */
	

1:
	l.sfgtui r14,0x6665
	l.bf     1f
	l.nop
	l.ori	 r11,r11,0x200  /* this should pass r14 |= 0x200 */

1:
	/* r11 should be 0x390 */
	
	l.lwz	 r14,0x4(r1)
	l.lwz    r9,0x0(r1)
	l.jr	 r9
	l.addi   r1,r1,8
