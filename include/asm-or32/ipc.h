#ifndef _OR32_IPC_H
#define _OR32_IPC_H

/* 
 * These are used to wrap system calls on OR32.
 *
 * See arch/or32/kernel/sys_or32.c for ugly details..
 *
 * Same as x86 version.
 *
 */
#if 0 /*seems to be standard in linux/ipc.h now RGD?*/
struct ipc_kludge {
	struct msgbuf *msgp;
	long msgtyp;
};
#endif

#define SEMOP		 1
#define SEMGET		 2
#define SEMCTL		 3
#define SEMTIMEDOP       4
#define MSGSND		11
#define MSGRCV		12
#define MSGGET		13
#define MSGCTL		14
#define SHMAT		21
#define SHMDT		22
#define SHMGET		23
#define SHMCTL		24

#define IPCCALL(version,op)	((version)<<16 | (op))

#endif /* _OR32_IPC_H */
