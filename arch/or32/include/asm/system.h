/*
 * Based on:
 * include/asm-ppc/system.h
 * Copyright (C) 1999 Cort Dougan <cort@cs.nmt.edu>
 */
#ifndef __OR32_SYSTEM_H
#define __OR32_SYSTEM_H

#include <asm/processor.h>
#include <linux/linkage.h>

#include <linux/irqflags.h>

/* Memory bariers */
#define barrier() __asm__ __volatile__("": : :"memory")
#define mb() barrier()
#define rmb() mb()
#define wmb() mb()
#define read_barrier_depends() do { } while(0)
#define set_mb(var, value)  do { var = value; mb(); } while (0)
#define set_wmb(var, value) do { var = value; wmb(); } while (0)

#ifdef CONFIG_SMP
#define smp_mb()        mb()
#define smp_rmb()       rmb()
#define smp_wmb()       wmb()
#define smp_read_barrier_depends()     read_barrier_depends()
#else
#define smp_mb()        barrier()
#define smp_rmb()       barrier()
#define smp_wmb()       barrier()
#define smp_read_barrier_depends()     do { } while(0)
#endif

#define nop() __asm__ __volatile__ ("l.nop"::)

#ifdef __KERNEL__

struct task_struct;
extern unsigned int rtas_data;
struct pt_regs;

extern void show_regs(struct pt_regs * regs);

#if 0
extern void __save_flags(unsigned long *flags);
extern void __restore_flags(unsigned long flags);
extern void __save_and_cli(unsigned long *flags);
extern void __sti(void);
extern void __cli(void);


#define local_irq_disable()		__cli()
#define local_irq_enable()		__sti()
#define local_irq_save(flags)		__save_and_cli(&(flags))
#define local_irq_restore(flags)	__restore_flags(flags)
#define local_save_flags(flags)         __save_flags(&(flags))
#endif

#define prepare_to_switch()	do { } while(0)
#define switch_to(prev,next,last) _switch_to((prev),(next),&(last))

extern void _switch_to(struct task_struct *, struct task_struct *,
		       struct task_struct **);

struct __xchg_dummy { unsigned long a[100]; };
#define __xg(x) ((volatile struct __xchg_dummy *)(x))
#define xchg(ptr,x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))
#define tas(ptr) (xchg((ptr),1))

static inline unsigned long __xchg(unsigned long x, volatile void * ptr, int size)
{
  unsigned long tmp, flags;

  local_irq_save(flags);

  switch (size) {
  case 1:
    __asm__ __volatile__
    ("l.lbz %0,%2\n\t"
     "l.sb %2,%1"
    : "=&r" (tmp) : "r" (x), "m" (*__xg(ptr)) : "memory");
    break;
  case 2:
    __asm__ __volatile__
    ("l.lhz %0,%2\n\t"
     "l.sh %2,%1"
    : "=&r" (tmp) : "r" (x), "m" (*__xg(ptr)) : "memory");
    break;
  case 4:
    __asm__ __volatile__
    ("l.lwz %0,%2\n\t"
     "l.sw %2,%1"
    : "=&r" (tmp) : "r" (x), "m" (*__xg(ptr)) : "memory");
    break;
  }

  local_irq_restore(flags);
  return tmp;
}

static inline void * xchg_ptr(void * m, void * val)
{
	return (void *) __xchg((unsigned long)m, val, sizeof(unsigned long));
}

#define mtspr(_spr, _val) __asm__ __volatile__ ("l.mtspr r0,%1,%0" :: "K" (_spr), "r" (_val))
#define mtspr_off(_spr, _off, _val) __asm__ __volatile__ ("l.mtspr %0,%1,%2" :: "r" (_off), "r" (_val), "K" (_spr))

/*static inline void mtspr(unsigned short spr, unsigned long val) {
	__asm__ __volatile__ ("l.mtspr r0,%1,%0" :: "K" (spr), "r" (val)); 
}
*/

static inline unsigned long mfspr(unsigned long add)
{
	unsigned long ret;
	__asm__ __volatile__ ("l.mfspr %0,r0,%1" : "=r" (ret) : "K" (add));
	return ret;
}

//#define irqs_disabled() ((mfspr(SPR_SR) & (SPR_SR_IEE | SPR_SR_TEE)) == 0)

#define arch_align_stack(x) (x)

/*
 * debugging aid
 */
extern void __print(const char *fmt, ...);

#include <asm-generic/cmpxchg-local.h>

#endif /* __KERNEL__ */
#endif /* __OR32_SYSTEM_H */
