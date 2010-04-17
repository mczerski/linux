#ifdef __KERNEL__
#ifndef _OR32_CURRENT_H
#define _OR32_CURRENT_H

#if 0
/*
 * We keep `current' in r2 for speed.
 */
register struct task_struct *current asm ("r2");
#endif

#if 1

#include <linux/thread_info.h>

struct task_struct;

extern inline struct task_struct * get_current(void)
{
        return current_thread_info()->task;
}
 
#define current get_current()

#endif


#endif /* _OR32_CURRENT_H */
#endif /* __KERNEL__ */
