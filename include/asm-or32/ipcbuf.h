#ifndef _OR32_IPCBUF_H
#define _OR32_IPCBUF_H

/*
 * The ipc64_perm structure for or32 architecture.
 * Note extra padding because this structure is passed back and forth
 * between kernel and user space.
 */

struct ipc64_perm
{
	__kernel_key_t		key;
	__kernel_uid_t		uid;
	__kernel_gid_t		gid;
	__kernel_uid_t		cuid;
	__kernel_gid_t		cgid;
	__kernel_mode_t		mode;
	unsigned long		seq;
	unsigned int		__pad2;
	unsigned long long	__unused1;
	unsigned long long	__unused2;
};

#endif /* __OR32_IPCBUF_H__ */
