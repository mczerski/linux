/* asm/bitops.h for Linux/or32
 *
 * __PHX__ TODO: asm versions
 *
 */

#ifdef __KERNEL__
#ifndef _OR32_BITOPS_H
#define _OR32_BITOPS_H

#include <asm/system.h>
#include <asm/byteorder.h>
#include <linux/compiler.h>


static __inline__ int set_bit(int nr, volatile void * a)
{
	int 	* addr = (void *)a;
	int	mask, retval;
	unsigned long flags;

	addr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	local_irq_save(flags);
	retval = (mask & *addr) != 0;
	*addr |= mask;
	local_irq_restore(flags);
	return retval;
}

/*
 * non-atomic version
 */
static __inline__ void __set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);

	*p |= mask;
}

static __inline__ int clear_bit(int nr, volatile void * a)
{
	int 	* addr = (void *)a;
	int	mask, retval;
	unsigned long flags;

	addr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	local_irq_save(flags);
	retval = (mask & *addr) != 0;
	*addr &= ~mask;
	local_irq_restore(flags);
	return retval;
}

/*
 * non-atomic version
 */
static __inline__ void __clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);

	*p &= ~mask;
}

static __inline__ unsigned long change_bit(unsigned long nr,  void *addr)
{
	int mask;
  unsigned long flags;
	unsigned long *ADDR = (unsigned long *) addr;
	unsigned long oldbit;

	ADDR += nr >> 5;
	mask = 1 << (nr & 31);
	local_irq_save(flags);
	oldbit = (mask & *ADDR);
	*ADDR ^= mask;
	local_irq_restore(flags);
	return oldbit != 0;
}

/*
 * non-atomic version
 */
static __inline__ void __change_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);

	*p ^= mask;
}

static __inline__ int test_bit(int nr, const void *a)
{
	unsigned int mask;
        unsigned int *adr = (unsigned int *)a;
         
        adr += nr >> 5;
        mask = 1 << (nr & 0x1f);
        return ((mask & *adr) != 0);
}

static __inline__ int test_and_set_bit(int nr, void *addr)
{
	unsigned int mask, retval;
	unsigned long flags;
	unsigned int *adr = (unsigned int *)addr;
	
	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	local_irq_save(flags);
	retval = (mask & *adr) != 0;
	*adr |= mask;
	local_irq_restore(flags);
	return retval;
}

/*
 * non-atomic version
 */
static __inline__ int __test_and_set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);
	unsigned long old = *p;

	*p = old | mask;
	return (old & mask) != 0;
}


static __inline__ int test_and_clear_bit(int nr, void *addr)
{
	unsigned int mask, retval;
	unsigned long flags;
	unsigned int *adr = (unsigned int *)addr;
	
	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	local_irq_save(flags);
	retval = (mask & *adr) != 0;
	*adr &= ~mask;
	local_irq_restore(flags);
	return retval;
}

/*
 * non-atomic version
 */
static __inline__ int __test_and_clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);
	unsigned long old = *p;

	*p = old & ~mask;
	return (old & mask) != 0;
}

static __inline__ int test_and_change_bit(int nr, void *addr)
{
	unsigned int mask, retval;
	unsigned long flags;
	unsigned int *adr = (unsigned int *)addr;
	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	local_irq_save(flags);
	retval = (mask & *adr) != 0;
	*adr ^= mask;
	local_irq_restore(flags);
	return retval;
}

/*
 * non-atomic version
 */
static __inline__ int __test_and_change_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);
	unsigned long old = *p;

	*p = old ^ mask;
	return (old & mask) != 0;
}

#define __change_bit(nr, addr) (void)__test_and_change_bit(nr, addr)

/*
 * Find-bit routines..
 */

/*
 * fls: find last bit set.
 */

#define fls(x) generic_fls(x)

/* The easy/cheese version for now. */
static __inline__ unsigned long ffz(unsigned long word)
{
	unsigned long result = 0;

	while(word & 1) {
		result++;
		word >>= 1;
	}
	return result;
}

/*
 * ffs: find first bit set. This is defined the same way as
 * the libc and compiler builtin ffs routines, therefore
 * differs in spirit from the above ffz (man ffs).
 */

#define ffs(x) generic_ffs(x)

/*
 * hweightN - returns the hamming weight of a N-bit word
 * @x: the word to weigh
 *
 * The Hamming Weight of a number is the total number of bits set in it.
 */
#if 0 /*RGD*/
#define hweight32(x) generic_hweight32(x)
#define hweight16(x) generic_hweight16(x)
#define hweight8(x) generic_hweight8(x)
#endif
/**
 * __ffs - find first bit in word.
 * @word: The word to search
 *
 * Undefined if no bit exists, so code should check against 0 first.
 */
static __inline__ int __ffs(unsigned long word)
{
	int num = 0;

	if ((word & 0xffff) == 0) {
		num += 16;
		word >>= 16;
	}
	if ((word & 0xff) == 0) {
		num += 8;
		word >>= 8;
	}
	if ((word & 0xf) == 0) {
		num += 4;
		word >>= 4;
	}
	if ((word & 0x3) == 0) {
		num += 2;
		word >>= 2;
	}
	if ((word & 0x1) == 0)
		num += 1;
	return num;
}

/*
 * Every architecture must define this function. It's the fastest
 * way of searching a 140-bit bitmap where the first 100 bits are
 * unlikely to be set. It's guaranteed that at least one of the 140
 * bits is cleared.
 */
static __inline__ int sched_find_first_bit(unsigned long *b)
{

	if (unlikely(b[0]))
		return __ffs(b[0]);
	if (unlikely(b[1]))
		return __ffs(b[1]) + 32;
	if (unlikely(b[2]))
		return __ffs(b[2]) + 64;
	if (b[3])
		return __ffs(b[3]) + 96;
	return __ffs(b[4]) + 128;
}

/**
 *  * find_next_bit - find the next set bit in a memory region
 *  * @addr: The address to base the search on
 *  * @offset: The bitnumber to start searching at
 *  * @size: The maximum size to search
 *  */
static __inline__ unsigned long find_next_bit(const unsigned long *addr,
					              unsigned long size, unsigned long offset)
{
	unsigned int *p = ((unsigned int *) addr) + (offset >> 5);
	unsigned int result = offset & ~31UL;
	unsigned int tmp;
	
	if (offset >= size)
	  return size;
	size -= result;
	offset &= 31UL;
	if (offset) {
		tmp = *p++;
		tmp &= ~0UL << offset;
		if (size < 32)
		  goto found_first;
		if (tmp)
		  goto found_middle;
		size -= 32;
		result += 32;
	}
	while (size >= 32) {
		if ((tmp = *p++) != 0)
		  goto found_middle;
		result += 32;
		size -= 32;
	}
	if (!size)
	  return result;
	tmp = *p;
	
	found_first:
	tmp &= ~0UL >> (32 - size);
	if (tmp == 0UL)        /* Are any bits set? */
	  return result + size; /* Nope. */
	found_middle:
	return result + __ffs(tmp);
}


/* find_next_zero_bit() finds the first zero bit in a bit string of length
 * 'size' bits, starting the search at bit 'offset'. This is largely based
 * on Linus's ALPHA routines, which are pretty portable BTW.
 */

static __inline__ unsigned long find_next_zero_bit(const unsigned long *addr,
						   unsigned long size, 
						   unsigned long offset)
{
	unsigned long *p = ((unsigned long *) addr) + (offset >> 5);
	unsigned long result = offset & ~31UL;
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 31UL;
	if (offset) {
		tmp = *(p++);
		tmp |= ~0UL >> (32-offset);
		if (size < 32)
			goto found_first;
		if (~tmp)
			goto found_middle;
		size -= 32;
		result += 32;
	}
	while (size & ~31UL) {
		if (~(tmp = *(p++)))
			goto found_middle;
		result += 32;
		size -= 32;
	}
	if (!size)
		return result;
	tmp = *p;

found_first:
	tmp |= ~0UL >> size;
found_middle:
	return result + ffz(tmp);
}

/**
 * find_first_bit - find the first set bit in a memory region
 * @addr: The address to start the search at
 * @size: The maximum size to search
 *
 * Returns the bit-number of the first set bit, not the number of the byte
 * containing a bit.
 */
#define find_first_bit(addr, size) \
        find_next_bit((addr), (size), 0)
               

/* Linus sez that gcc can optimize the following correctly, we'll see if this
 * holds on the Sparc as it does for the ALPHA.
 */

#define find_first_zero_bit(addr, size) \
        find_next_zero_bit((addr), (size), 0)

/* Now for the ext2 filesystem bit operations and helper routines. */

static __inline__ int ext2_set_bit(int nr,void * addr)
{
	int		mask, retval;
  unsigned long flags;
	unsigned char	*ADDR = (unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	local_irq_save(flags);
	retval = (mask & *ADDR) != 0;
	*ADDR |= mask;
	local_irq_restore(flags);
	return retval;
}

static __inline__ int ext2_clear_bit(int nr, void * addr)
{
	int		mask, retval;
  unsigned long flags;
	unsigned char	*ADDR = (unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	local_irq_save(flags);
	retval = (mask & *ADDR) != 0;
	*ADDR &= ~mask;
	local_irq_restore(flags);
	return retval;
}

static __inline__ int ext2_test_bit(int nr, const void * addr)
{
	int			mask;
	const unsigned char	*ADDR = (const unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	return ((mask & *ADDR) != 0);
}

#define ext2_find_first_zero_bit(addr, size) \
        ext2_find_next_zero_bit((addr), (size), 0)

static __inline__ unsigned long ext2_find_next_zero_bit(void *addr, unsigned long size, unsigned long offset)
{
	unsigned long *p = ((unsigned long *) addr) + (offset >> 5);
	unsigned long result = offset & ~31UL;
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 31UL;
	if(offset) {
		tmp = *(p++);
		tmp |= ~0UL << (32-offset);
		if(size < 32)
			goto found_first;
		if(~tmp)
			goto found_middle;
		size -= 32;
		result += 32;
	}
	while(size & ~31UL) {
		if(~(tmp = *(p++)))
			goto found_middle;
		result += 32;
		size -= 32;
	}
	if(!size)
		return result;
	tmp = *p;

found_first:
	tmp |= ~0UL << size;
found_middle:
	tmp = ((tmp>>24) | ((tmp>>8)&0xff00) | ((tmp<<8)&0xff0000) | (tmp<<24));
	return result + ffz(tmp);
}

#define __ext2_set_bit ext2_set_bit
#define __ext2_clear_bit ext2_clear_bit

static __inline__ int __ext2_test_bit(int nr, __const__ void * addr)
{
	int			mask;
	__const__ unsigned char	*ADDR = (__const__ unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	return ((mask & *ADDR) != 0);
}

#define __ext2_find_first_zero_bit(addr, size) \
        __ext2_find_next_zero_bit((addr), (size), 0)

static __inline__ unsigned long __ext2_find_next_zero_bit(void *addr, unsigned long size, unsigned long offset)
{
	unsigned long *p = ((unsigned long *) addr) + (offset >> 5);
	unsigned long result = offset & ~31UL;
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 31UL;
	if(offset) {
		tmp = *(p++);
		tmp |= __swab32(~0UL >> (32-offset));
		if(size < 32)
			goto found_first;
		if(~tmp)
			goto found_middle;
		size -= 32;
		result += 32;
	}
	while(size & ~31UL) {
		if(~(tmp = *(p++)))
			goto found_middle;
		result += 32;
		size -= 32;
	}
	if(!size)
		return result;
	tmp = *p;

found_first:
	return result + ffz(__swab32(tmp) | (~0UL << size));
found_middle:
	return result + ffz(__swab32(tmp));
}

#define ext2_set_bit_atomic(lock, nr, addr)             \
        ({                                              \
                int ret;                                \
                spin_lock(lock);                        \
                ret = ext2_set_bit((nr), (unsigned long *)(addr)); \
                spin_unlock(lock);                      \
                ret;                                    \
        })
 
#define ext2_clear_bit_atomic(lock, nr, addr)           \
        ({                                              \
                int ret;                                \
                spin_lock(lock);                        \
                ret = ext2_clear_bit((nr), (unsigned long *)(addr)); \
                spin_unlock(lock);                      \
                ret;                                    \
        })

/*
 * clear_bit() doesn't provide any barrier for the compiler.
 */

#define smp_mb__before_clear_bit()      barrier()
#define smp_mb__after_clear_bit()       barrier()
#include <asm-generic/bitops/ffs.h>
#include <asm-generic/bitops/fls.h>
#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/fls64.h>
#include <asm-generic/bitops/lock.h>
#endif /* _OR32_BITOPS_H */
#endif /* __KERNEL__ */
