/*
 *  linux/include/asm-ba/alternatives.h
 *
 *  BA version
 *    author(s): Gyorgy Jeney (nog@beyondsemi.com)
 *
 *  For more information about BA processors, licensing and
 *  design services you may contact Beyond Semiconductor at
 *  sales@bsemi.com or visit website http://www.bsemi.com.
 *
 *  changes:
 *  04. 07. 2007: Gyorgy Jeney (nog@beyondsemi.com)
 *    Initial bright spark idea
 */

/* Code alternatives.  This is actually simple self-modifing code support.. */

#ifndef __ASSEMBLY__

struct code_alt_fixup {
	unsigned long where;
	unsigned long what;
	unsigned int type;
};

struct code_alt {
	unsigned long alt; /* where do we find the alternate sequence */
	unsigned int len; /* Length of the alternate sequence */
};

struct code_alts {
	unsigned long where; /* Where to overwrite this stuff.. */
	unsigned int space; /* How much space do we have */
	struct code_alt alt[];
};

void __switch_alternate(struct code_alts *alt, int num, int inv_mmu);

/* Switches to an alternative code sequence */
#define switch_alternate(alt, num) \
	do { \
		extern struct code_alts __alt_ ## alt; \
		__switch_alternate(&__alt_ ## alt, num, 0); \
	} while(0)

/* Same as switch_alternate, but this makes sure to invalidate the mmus
 * atomically with the code switch.  This is needed when switching the tlb
 * miss handlers since the boot-time handlers make sure to set the _CI bit
 * on all pages, which may cause problems when the real handler doesn't force
 * _CI */
#define switch_alternate_mmu(alt, num) \
	do { \
		extern struct code_alts __alt_ ## alt; \
		__switch_alternate(&__alt_ ## alt, num, 1); \
	} while(0)


#else /* __ASSEMBLY__ */

/* Assembly helpers */

/* Constant value alternatives (replaces the immediate value of the
 * instruction) */
#define CV_ALT(insn, group) \
	9999: insn ;\
	.section .cv_alt. ## group ;\
	.word 9999b+2 ;\
	.previous

/* Code alternatives
 *
 * Take your assembly code and do:
 * C_ALT_BEGIN(my_alt)
 *  b.xxx
 *  ...  <--- The default and alternate 0 code here
 * C_ALT
 *  b.yyy
 *  ...  <--- Alternate 1 code here
 * C_ALT
 *  b.zzz
 *  ...  <--- Alternate 2 code here
 * C_ALT_END
 *
 * You may have as many alternates as you want.  Should an alternate be longer
 * than the default, the space after the default shall be padded.  However,
 * note that for this to work, you need an assembler that fills spaces with
 * nop's, which doesn't exist yet..
 *
 * Jumps/branches that exit the alternate sequence need to be fixed up for the
 * alternate to work, thus these must be specified as
 * C_ALT_L_EXT_JUMP(j, _target). Any of jal, j, bf or bnf may be specified.
 * If the branch is local to the alternative, nothing needs to be done. */

/* The default needs to get the target fixed up, but for everything
 * else, it is not needed and the linker will potentially complain
 * about overflows */
#ifdef CONFIG_OPENRISC_BA2
# define C_ALT_L_EXT_JUMP(insn, target) \
	99997: ;\
	.if __c_alt_we_are_def==1 ;\
		bw.insn target ;\
	.else ;\
		bw.insn 0 ;\
	.endif ;\
	;\
	.pushsection .c_alt_fixups ;\
	.word 99997b+2 ;\
	.word target ;\
	.word 16 /* R_OR32_32_PCREL */ ;\
	.popsection
#else
# define C_ALT_L_EXT_JUMP(insn, target) \
	99997: ;\
	.if __c_alt_we_are_def==1 ;\
		l.insn target ;\
	.else ;\
		l.insn 0 ;\
	.endif ;\
	;\
	.pushsection .c_alt_fixups ;\
	.word 99997b ;\
	.word target ;\
	.word 8 /* R_OR32_JUMPTARG */ ;\
	.popsection
#endif

/* Wow, I never knew I could be so abuseive.. */
#define C_ALT_BEGIN(name) \
	/* Make sure the sections have the correct attributes */ \
	.section .c_alt. ## name,"a" ;\
	.previous ;\
	.section .c_alts,"a" ;\
	.previous ;\
	.section .c_alt_fixups,"a" ;\
	.previous ;\
	;\
	.equ __c_alt_num,1 ;\
	.equ __c_alt_max_size,0 ;\
	;\
	.macro __c_inst_alt ;\
		.pushsection .c_alt. ## name ;\
		9998: ;\
		__c_insn_seq ;\
		9999: ;\
		.popsection ;\
		;\
		.pushsection .c_alts ;\
		.word 9998b ;\
		.word 9999b-9998b ;\
		.popsection ;\
		;\
		.if __c_alt_max_size < (9999b-9998b) ;\
			.equ __c_alt_max_size,9999b-9998b ;\
		.endif ;\
	.endm ;\
	;\
	99998:;\
	.pushsection .c_alts ;\
	___alt_ ## name : ;\
	.globl ___alt_ ## name ;\
	.word	99998b ;\
	.word	99999f-99998b ;\
	.popsection ;\
	.macro __c_insn_seq

#define C_ALT \
	.endm ;\
	;\
	__c_inst_alt ;\
	;\
	.if __c_alt_num==1 ;\
		.equ __c_alt_def_size,9999b-9998b ;\
		.equ __c_alt_we_are_def,1 ;\
		__c_insn_seq ;\
		.equ __c_alt_we_are_def,0 ;\
	.endif ;\
	;\
	.equ __c_alt_num,__c_alt_num+1 ;\
	;\
	.purgem __c_insn_seq ;\
	.macro __c_insn_seq

#define C_ALT_END \
	C_ALT ;\
	.endm ;\
	;\
	.if __c_alt_max_size!=__c_alt_def_size ;\
		.space __c_alt_max_size-__c_alt_def_size ;\
	.endif ;\
	99999:;\
	;\
	.purgem __c_insn_seq ;\
	.purgem __c_inst_alt

#endif /* __ASSEMBLY__ */
