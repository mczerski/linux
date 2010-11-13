/*
 *  linux/arch/or32/kernel/or32_defs.c
 *
 *  or32 version
 *  
 *  For more information about OpenRISC processors, licensing and
 *  design services you may contact Beyond Semiconductor at
 *  sales@bsemi.com or visit website http://www.bsemi.com.
 *
 * This program is used to generate definitions needed by
 * assembly language modules.
 *
 * We use the technique used in the OSF Mach kernel code:
 * generate asm statements containing #defines,
 * compile this file to assembler, and then extract the
 * #defines from the assembly-language output.
 */

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/thread_info.h>

#define DEFINE(sym, val) \
        asm volatile("\n->" #sym " %0 " #val : : "i" (val))
        
#define BLANK() asm volatile("\n->" : : )
        
int
main(void)
{
  /* offsets into the task_struct */
  DEFINE(TASK_STATE, offsetof(struct task_struct, state));
  DEFINE(TASK_FLAGS, offsetof(struct task_struct, flags));
  DEFINE(TASK_PTRACE, offsetof(struct task_struct, ptrace));
  DEFINE(TASK_THREAD, offsetof(struct task_struct, thread));
  DEFINE(TASK_MM, offsetof(struct task_struct, mm));
  DEFINE(TASK_ACTIVE_MM, offsetof(struct task_struct, active_mm));
  
  /* offsets into thread_info */
  DEFINE(TI_TASK, offsetof(struct thread_info, task));
  DEFINE(TI_FLAGS, offsetof(struct thread_info, flags));
  DEFINE(TI_PREEMPT, offsetof(struct thread_info, preempt_count));
  DEFINE(TI_KSP, offsetof(struct thread_info, ksp));
  
  /* Interrupt register frame */
  DEFINE(STACK_FRAME_OVERHEAD, STACK_FRAME_OVERHEAD);
  DEFINE(INT_FRAME_SIZE, STACK_FRAME_OVERHEAD + sizeof(struct pt_regs));
  
  DEFINE(NUM_USER_SEGMENTS, TASK_SIZE>>28);
  return 0;
}
