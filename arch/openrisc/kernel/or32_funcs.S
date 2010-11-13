/*
 *  linux/arch/or32/kernel/or32_funcs.S
 *
 *  or32 version
 *    author(s): Matjaz Breskvar (phoenix@bsemi.com)
 *
 *  For more information about OpenRISC processors, licensing and
 *  design services you may contact Beyond Semiconductor at
 *  sales@bsemi.com or visit website http://www.bsemi.com.
 *
 *  some helper functions for or32 assembler programming
 *
 */

#ifndef ENTRY
#  define ENTRY(symbol)			\
	  .global symbol		;\
   symbol:	
#endif
	
#define CLEAR_GPR(gpr)			\
    l.or    gpr,r0,r0

#define LOAD_SYMBOL_2_GPR(gpr,symbol)	\
    l.movhi gpr,hi(symbol)		;\
    l.ori   gpr,gpr,lo(symbol)

/*
 * DSCR: lower bits of SPR_SR defined by mask will be set to 1
 *
 * PRMS: t1 is temporary (and destroyed)
 */
#define SR_ENABLE_LO_BITS(mask,t1)	\
    l.mfspr t1,r0,SPR_SR		;\
    l.ori   t1,t1,lo(mask)		;\
    l.mtspr r0,t1,SPR_SR

/*
 * DSCR: bits set in mask will be set to 1
 *
 * PRMS: t1 is temporary register
 *       t2 is temporary register
 */
#define SR_ENABLE_BITS(mask,t1,t2)	\
    l.mfspr t2,r0,SPR_SR		;\
    LOAD_SYMBOL_2_GPR(t1,mask)		;\
    l.or    t2,t2,t1			;\
    l.mtspr r0,t2,SPR_SR

/*
 * DSCR: bits set in mask will be set to 0
 *
 * PRMS: t1 is temporary register
 *       t2 is temporary register
 */ 
#define SR_DISABLE_BITS(mask,t1,t2)	\
    l.mfspr t2,r0,SPR_SR		;\
    LOAD_SYMBOL_2_GPR(t1,(~mask))	;\
    l.and   t2,t2,t1			;\
    l.mtspr r0,t2,SPR_SR


/*
 * DSCR: lower bits of SPR defined by mask will be set to 1
 *
 * PRMS: t1 is temporary (and destroyed)
 */
#define SPR_ENABLE_LO_BITS(SPR,mask,t1) \
    l.mfspr t1,r0,SPR			;\
    l.ori  t1,t1,lo(mask)		;\
    l.mtspr r0,t1,SPR
	
/*
 * DSCR: lower bits of SPR defined by mask will be set to 0
 *
 * PRMS: t1 is temporary (and destroyed)
 */
#define SPR_DISABLE_LO_BITS(SPR,mask,t1) \
    l.mfspr t1,r0,SPR			;\
    l.andi  t1,t1,lo(~mask)		;\
    l.mtspr r0,t1,SPR
	

#define DISABLE_INTERRUPTS(t1,t2)	\
    SR_DISABLE_BITS((SPR_SR_IEE|SPR_SR_TEE),t1,t2)

#define ENABLE_INTERRUPTS(t1)		\
    SR_ENABLE_LO_BITS((SPR_SR_IEE|SPR_SR_TEE),t1)

