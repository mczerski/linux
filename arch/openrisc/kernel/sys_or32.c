/*
 *  linux/arch/or32/kernel/sys_or32.c
 *
 *  or32 version
 *    author(s): Matjaz Breskvar (phoenix@bsemi.com)
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
 * based on: linux/arch/cris/kernel/sys_cris.c
 *
 * This file contains various random system calls that
 * have a non-standard calling sequence on some platforms.
 * Since we don't have to do any backwards compatibility, our
 * versions are done in the most "normal" way possible.
 *
 */

#include <linux/errno.h>
#include <linux/syscalls.h>
#include <linux/mm.h>

#include <asm/syscalls.h>

asmlinkage long sys_mmap2(unsigned long addr, unsigned long len,
        unsigned long prot, unsigned long flags,
        unsigned long fd, unsigned long pgoff)
{
        return sys_mmap_pgoff(addr, len, prot, flags, fd, pgoff);
}

asmlinkage long sys_mmap(unsigned long addr, unsigned long len, 
                        unsigned long prot, unsigned long flags, 
                        unsigned long fd, off_t pgoff) 
{ 
        if (pgoff & ~PAGE_MASK) 
                return -EINVAL; 

        return sys_mmap_pgoff(addr, len, prot, flags, fd, pgoff >> PAGE_SHIFT); 
}

/* These are secondary entry points as the primary entry points are defined in
 * entry.S where we add the 'regs' parameter value 
 */

asmlinkage long _sys_clone(unsigned long clone_flags, unsigned long newsp,
                        int __user *parent_tid, int __user *child_tid,
                        struct pt_regs *regs)
{
	long ret;

	/* FIXME: Is alignment necessary? */
	/* newsp = ALIGN(newsp, 4); */

	if (!newsp)
		newsp = regs->sp;

	ret = do_fork(clone_flags, newsp, regs, 0, parent_tid, child_tid);

	return ret;
}

asmlinkage int _sys_fork(struct pt_regs *regs)
{
#ifdef CONFIG_MMU
        return do_fork(SIGCHLD, regs->sp, regs, 0, NULL, NULL);
#else
	return -EINVAL;
#endif
}

asmlinkage int _sys_vfork(struct pt_regs *regs)
{
	/* This doesn't seem to work */
//        return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs->sp, regs, 0, NULL, NULL);

	/* This works */
        return do_fork(SIGCHLD, regs->sp, regs, 0, NULL, NULL);
}


