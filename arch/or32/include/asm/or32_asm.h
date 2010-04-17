/*
 * include/asm-or32/or32_asm.h
 */


#define tophys(rd,rs)				\
	l.movhi	rd,hi(-KERNELBASE);		\
  l.add   rd,rd,rs;

#define tovirt(rd,rs)				\
	l.movhi	rd,hi(KERNELBASE);		\
  l.add   rd,rd,rs;


