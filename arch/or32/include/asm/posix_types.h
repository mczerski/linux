#ifndef __ASM_OPENRISC_POSIX_TYPES_H
#define __ASM_OPENRISC_POSIX_TYPES_H

/* These definitions are different from the defaults and override the
 * definitions in the generic header.  For everything else, the generic
 * header applies.
 */

/* We can get rid of these defines, but we need to change in libC also */
/*
typedef unsigned int    __kernel_ino_t;
#define __kernel_ino_t __kernel_ino_t

typedef unsigned short  __kernel_nlink_t;
#define __kernel_nlink_t __kernel_nlink_t

typedef short           __kernel_ipc_pid_t;
#define __kernel_ipc_pid_t __kernel_ipc_pid_t

typedef unsigned short  __kernel_old_dev_t;
#define __kernel_old_dev_t __kernel_old_dev_t
*/
#include <asm-generic/posix_types.h>

#endif /* __ASM_OPENRISC_POSIX_TYPES_H */
