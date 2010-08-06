#ifndef __ASM_OPENRISC_ELF_H
#define __ASM_OPENRISC_ELF_H

/*
 * ELF register definitions..
 */
#include <asm/types.h>
#include <asm/ptrace.h>

typedef unsigned long elf_greg_t;

/* Note that NGREG is defined to ELF_NGREG in include/linux/elfcore.h, and is
   thus exposed to user-space. */
#define ELF_NGREG (sizeof (struct pt_regs) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

/* A placeholder; OR32 does not have fp support yes, so no fp regs for now.  */
typedef unsigned long elf_fpregset_t;

/* This should be moved to include/linux/elf.h */
#define EM_OR32         0x8472
#define EM_OPENRISC     92     /* OpenRISC 32-bit embedded processor */

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_ARCH	EM_OR32
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2MSB

#ifdef __KERNEL__

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */

#define elf_check_arch(x) \
       (((x)->e_machine == EM_OR32) || ((x)->e_machine == EM_OPENRISC))

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#define ELF_ET_DYN_BASE         (0x08000000)

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE	8192

#define ELF_CORE_COPY_REGS(gregs, regs) \
	memcpy(gregs, regs, \
	       sizeof(struct pt_regs) < sizeof(elf_gregset_t)? \
	       sizeof(struct pt_regs): sizeof(elf_gregset_t));


/* This yields a mask that user programs can use to figure out what
   instruction set this cpu supports.  This could be done in userspace,
   but it's not easy, and we've already done it here.  */

#define ELF_HWCAP	(0)

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo.

   For the moment, we have only optimizations for the Intel generations,
   but that could change... */

#define ELF_PLATFORM	(NULL)

#define SET_PERSONALITY(ex) set_personality(PER_LINUX)

#endif /* __KERNEL__ */
#endif
