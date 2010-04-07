/*
 * based on ppc, cris
 * 
 * 
 */

#ifdef __KERNEL__
#ifndef _OR32_CHECKSUM_H
#define _OR32_CHECKSUM_H

#include <asm/spr_defs.h>

/*
 * computes the checksum of a memory block at buff, length len,
 * and adds in "sum" (32-bit)
 *
 * returns a 32-bit number suitable for feeding into itself
 * or csum_tcpudp_magic
 *
 * this function must be called with even lengths, except
 * for the last fragment, which may be odd
 *
 * it's best to have buff aligned on a 32-bit boundary
 */
extern unsigned int csum_partial(const unsigned char * buff, int len,
				 unsigned int sum);

/*
 * Computes the checksum of a memory block at src, length len,
 * and adds in "sum" (32-bit), while copying the block to dst.
 * If an access exception occurs on src or dst, it stores -EFAULT
 * to *src_err or *dst_err respectively (if that pointer is not
 * NULL), and, for an error on src, zeroes the rest of dst.
 *
 * Like csum_partial, this must be called with even lengths,
 * except for the last fragment.
 */
unsigned int csum_partial_copy_from_user (const char *src, char *dst,
                                          int len, unsigned int sum,
                                          int *err_ptr);
	                                         
unsigned int csum_partial_copy_nocheck(const char *src, char *dst,
                                       int len, unsigned int sum);
                                       					
/*
 * Old versions which ignore errors.
 */
#define csum_partial_copy(src, dst, len, sum)	\
	csum_partial_copy_nocheck((src), (dst), (len), (sum))

/*
 * turns a 32-bit partial checksum (e.g. from csum_partial) into a
 * 1's complement 16-bit checksum.
 */
static inline unsigned int csum_fold(unsigned int sum)
{
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);
	return ~sum;
}

static inline unsigned long csum_tcpudp_nofold(unsigned long saddr,
					       unsigned long daddr,
					       unsigned short len,
					       unsigned short proto,
					       unsigned int sum) 
{
	sum += saddr;
	sum += (saddr > sum) ? 1 : 0;

	sum += daddr;
	sum += (daddr > sum) ? 1 : 0;

	sum += ((proto<<16)+len);
	sum += (((proto<<16)+len) > sum) ? 1 : 0;

	return sum;
}

/*
 *  This is a version of ip_compute_csum() optimized for IP headers,
 *  which always checksum on 4 octet boundaries.
 */

static inline unsigned short ip_fast_csum(unsigned char * iph,
					  unsigned int ihl)
{
	return csum_fold(csum_partial(iph, ihl * 4, 0));
}

/*
 * computes the checksum of the TCP/UDP pseudo-header
 * returns a 16-bit checksum, already complemented
 */

static inline unsigned short int csum_tcpudp_magic(unsigned long saddr,
						   unsigned long daddr,
						   unsigned short len,
						   unsigned short proto,
						   unsigned int sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr,daddr,len,proto,sum));
}

/*
 * this routine is used for miscellaneous IP-like checksums, mainly
 * in icmp.c
 */

static inline unsigned short ip_compute_csum(unsigned char * buff, int len)
{
	return csum_fold (csum_partial(buff, len, 0));
}


#endif /* __KERNEL__ */
#endif /* _OR32_CHECKSUM_H */
