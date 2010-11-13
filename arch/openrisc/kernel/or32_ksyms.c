/*
 *  linux/arch/or32/kernel/or32_ksyms.c
 *
 *  or32 version
 *
 *  For more information about OpenRISC processors, licensing and
 *  design services you may contact Beyond Semiconductor at
 *  sales@bsemi.com or visit website http://www.bsemi.com.
 *
 *  derived from cris, i386, m68k, ppc, sh ports.
 *
 *  changes:
 *  18. 11. 2003: Matjaz Breskvar (phoenix@bsemi.com)
 *    initial port to or32 architecture
 *
 */

#include <linux/module.h>
#include <linux/elfcore.h>
#include <linux/sched.h>
#include <linux/in6.h>
#include <linux/interrupt.h>
#include <linux/smp_lock.h>
#include <linux/vmalloc.h>
#include <linux/semaphore.h>

#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/checksum.h>
#include <asm/io.h>
#include <asm/hardirq.h>
#include <asm/delay.h>
#include <asm/pgalloc.h>


#define DECLARE_EXPORT(name) extern void name(void);EXPORT_SYMBOL(name)

/* compiler generated symbols */
DECLARE_EXPORT(__udivsi3);
DECLARE_EXPORT(__divsi3);
DECLARE_EXPORT(__umodsi3);
DECLARE_EXPORT(__modsi3);
DECLARE_EXPORT(__muldi3);
DECLARE_EXPORT(__ashrdi3);
DECLARE_EXPORT(__ashldi3);
DECLARE_EXPORT(__lshrdi3);

EXPORT_SYMBOL(__copy_tofrom_user);

