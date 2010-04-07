#ifndef __ASM_OR32_ATOMIC__
#define __ASM_OR32_ATOMIC__

#include <asm/system.h>
#include <linux/linkage.h>
#include <linux/compiler.h>
/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 */

/*
 * Make sure gcc doesn't try to be clever and move things around
 * on us. We need to use _exactly_ the address the user gave us,
 * not some alias that contains the same information.
 */

#define __atomic_fool_gcc(x) (*(struct { int a[100]; } *)x)

typedef struct { int counter; } atomic_t;

#define ATOMIC_INIT(i)  { (i) }

#define atomic_read(v) ((v)->counter)
#define atomic_set(v,i) (((v)->counter) = (i))

/* These should be written in asm but we do it in C for now. */

extern __inline__ void atomic_add(int i, volatile atomic_t *v)
{
	unsigned long flags;
	local_save_flags(flags);
	local_irq_disable();
	v->counter += i;
	local_irq_restore(flags);
}

extern __inline__ void atomic_sub(int i, volatile atomic_t *v)
{
	unsigned long flags;
	local_save_flags(flags);
	local_irq_disable();
	v->counter -= i;
	local_irq_restore(flags);
}

extern __inline__ int atomic_add_return(int i, volatile atomic_t *v)
{
	unsigned long flags;
	int retval;
	local_save_flags(flags);
	local_irq_disable();
	retval = (v->counter += i);
	local_irq_restore(flags);
	return retval;
}

#define atomic_add_negative(a, v)	(atomic_add_return((a), (v)) < 0)

extern __inline__ int atomic_sub_return(int i, volatile atomic_t *v)
{
	unsigned long flags;
	int retval;
	local_save_flags(flags);
	local_irq_disable();
	retval = (v->counter -= i);
	local_irq_restore(flags);
	return retval;
}

extern __inline__ int atomic_sub_and_test(int i, volatile atomic_t *v)
{
	int retval;
	unsigned long flags;
	local_save_flags(flags);
	local_irq_disable();
	retval = (v->counter -= i) == 0;
	local_irq_restore(flags);
	return retval;
}

extern __inline__ void atomic_inc(volatile atomic_t *v)
{
	unsigned long flags;
	local_save_flags(flags);
	local_irq_disable();
	(v->counter)++;
	local_irq_restore(flags);
}

extern __inline__ void atomic_dec(volatile atomic_t *v)
{
	unsigned long flags;
	local_save_flags(flags);
	local_irq_disable();
	(v->counter)--;
	local_irq_restore(flags);
}

extern __inline__ int atomic_inc_return(volatile atomic_t *v)
{
	unsigned long flags;
	int retval;
	local_save_flags(flags);
	local_irq_disable();
	retval = (v->counter)++;
	local_irq_restore(flags);
	return retval;
}

extern __inline__ int atomic_dec_return(volatile atomic_t *v)
{
	unsigned long flags;
	int retval;
	local_save_flags(flags);
	local_irq_disable();
	retval = (v->counter)--;
	local_irq_restore(flags);
	return retval;
}
extern __inline__ int atomic_dec_and_test(volatile atomic_t *v)
{
	int retval;
	unsigned long flags;
	local_save_flags(flags);
	local_irq_disable();
	retval = --(v->counter) == 0;
	local_irq_restore(flags);
	return retval;
}

extern __inline__ int atomic_inc_and_test(volatile atomic_t *v)
{
	int retval;
	unsigned long flags;
	local_save_flags(flags);
	local_irq_disable();
	retval = ++(v->counter) == 0;
	local_irq_restore(flags);
	return retval;
}
/*RGD*/
static inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
	int ret;
	unsigned long flags;

	local_irq_save(flags);
	ret = v->counter;
	if (likely(ret == old))
		v->counter = new;
	local_irq_restore(flags);

	return ret;
}

/* Atomic operations are already serializing */
#define smp_mb__before_atomic_dec()    barrier()
#define smp_mb__after_atomic_dec()     barrier()
#define smp_mb__before_atomic_inc()    barrier()
#define smp_mb__after_atomic_inc()     barrier()
#define atomic_xchg(v, new) (xchg(&((v)->counter), new)) /*RGD*/

/*RGD*/
static inline int atomic_add_unless(atomic_t *v, int a, int u)
{
	int ret;
	unsigned long flags;

	local_save_flags(flags);
	local_irq_disable();
	ret = v->counter;
	if (ret != u)
		v->counter += a;
	local_irq_restore(flags);
	return ret != u;
}
#define atomic_inc_not_zero(v) atomic_add_unless((v), 1, 0)
/*RGD*/
/*RGD*/
#include <asm/errno.h>
#include <asm/uaccess.h>
static inline int
futex_atomic_cmpxchg_inatomic(int __user *uaddr, int oldval, int newval)
{
	return -ENOSYS;
}
/*RGD*/

#include <asm-generic/atomic.h> /*RGD*/
#endif /* __ASM_OR32_ATOMIC__ */
