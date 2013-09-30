/*
2.16.2004, rework of the uncopyrighted Linux msg.h files for RTAI
hard real time by: Paolo Mantegazza <mantegazza@aero.polimi.it>

COPYRIGHT (C) 2004  Paolo Mantegazza <mantegazza@aero.polimi.it>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
*/

#ifndef _RTAI_SYSV_MSG_H
#define _RTAI_SYSV_MSG_H

#define SYSV_MSG_IDX  1

#define SYSV_MSGGET   0
#define SYSV_MSGCTL   1
#define SYSV_MSGSND   2
#define SYSV_MSGRCV   3

#ifdef __KERNEL__

//#include <linux/ipc.h>

/* ipcs ctl commands */
#define MSG_STAT 11
#define MSG_INFO 12

/* msgrcv options */
#define MSG_NOERROR     010000  /* no error if message is too big */
#define MSG_EXCEPT      020000  /* recv any msg except of specified type.*/

/* Obsolete, used only for backwards compatibility and libc5 compiles */
struct msqid_ds {
	struct ipc_perm msg_perm;
	struct msg *msg_first;		/* first message on queue,unused  */
	struct msg *msg_last;		/* last message in queue,unused */
	__kernel_time_t msg_stime;	/* last msgsnd time */
	__kernel_time_t msg_rtime;	/* last msgrcv time */
	__kernel_time_t msg_ctime;	/* last change time */
	unsigned long  msg_lcbytes;	/* Reuse junk fields for 32 bit */
	unsigned long  msg_lqbytes;	/* ditto */
	unsigned short msg_cbytes;	/* current number of bytes on queue */
	unsigned short msg_qnum;	/* number of messages in queue */
	unsigned short msg_qbytes;	/* max number of bytes on queue */
	__kernel_ipc_pid_t msg_lspid;	/* pid of last msgsnd */
	__kernel_ipc_pid_t msg_lrpid;	/* last receive pid */
};

/* Include the definition of msqid64_ds */
#include <asm/msgbuf.h>

/* message buffer for msgsnd and msgrcv calls */
struct msgbuf {
	long mtype;         /* type of message */
	char mtext[1];      /* message text */
};

/* buffer for msgctl calls IPC_INFO, MSG_INFO */
struct msginfo {
	int msgpool;
	int msgmap;
	int msgmax;
	int msgmnb;
	int msgmni;
	int msgssz;
	int msgtql;
	unsigned short  msgseg;
};

#define MSGMNI    16   /* <= IPCMNI */     /* max # of msg queue identifiers */
#define MSGMAX  8192   /* <= INT_MAX */   /* max size of message (bytes) */
#define MSGMNB 16384   /* <= INT_MAX */   /* default max size of a message queue */

/* unused */
#define MSGPOOL (MSGMNI*MSGMNB/1024)  /* size in kilobytes of message pool */
#define MSGTQL  MSGMNB            /* number of system message headers */
#define MSGMAP  MSGMNB            /* number of entries in message map */
#define MSGSSZ  16                /* message segment size */
#define __MSGSEG ((MSGPOOL*1024)/ MSGSSZ) /* max no. of segments */
#define MSGSEG (__MSGSEG <= 0xffff ? __MSGSEG : 0xffff)

int rt_msgget (key_t key, int msgflg);

int _rt_msgctl(int msqid, int cmd, struct msqid_ds *buf, int space);

static inline int rt_msgctl(int msqid, int cmd, struct msqid_ds *buf)
{
	return _rt_msgctl(msqid, cmd, buf, 0);
}

int _rt_msgsnd(int msqid, int mtype, void *mtext, size_t msgsz, int msgflg, int space);

int _rt_msgrcv(int msqid, int *mtype, void *mtext, size_t msgsz, long msgtyp, int msgflg, int space);

// this is the way we like it ...

static inline int rt_msgsnd_nu(int msqid, int mtype, void *mtext, size_t msgsz, int msgflg)
{
	return _rt_msgsnd(msqid, mtype, mtext, msgsz, msgflg, 0);
}

static inline int rt_msgrcv_nu(int msqid, int *mtype, void *mtext, size_t msgsz, long msgtyp, int msgflg)
{
	return _rt_msgrcv(msqid, mtype, mtext, msgsz, msgtyp, msgflg, 0);
}

// ... and this is the Unix standard, i.e. using the left hand to scrap the
// right hear.

static inline int rt_msgsnd(int msqid, struct msgbuf *msgp, size_t msgsz, int msgflg)
{
	return _rt_msgsnd(msqid, msgp->mtype, &msgp->mtext, msgsz, msgflg, 0);
}

static inline int rt_msgrcv(int msqid, struct msgbuf *msgp, size_t msgsz, long msgtyp, int msgflg)
{
	return _rt_msgrcv(msqid, (int *)&msgp->mtype, msgp->mtext, msgsz, msgtyp, msgflg, 0);
}

#else /* !__KERNEL__ */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <rtai_lxrt.h>

static inline int rt_msgget(key_t key, unsigned long msgflg)
{
	struct { key_t key; unsigned long msgflg; } arg = { key, msgflg };
	return rtai_lxrt(SYSV_MSG_IDX, SIZARG, SYSV_MSGGET, &arg).i[LOW];
}

static inline int rt_msgctl(int msqid, int cmd, struct msqid_ds *buf)
{
	struct { int msqid, cmd; struct msqid_ds *buf; int space; } arg = { msqid, cmd, buf, 1 };
	return rtai_lxrt(SYSV_MSG_IDX, SIZARG, SYSV_MSGCTL, &arg).i[LOW];
}

struct msgbuf { long int mtype; char mtext[1]; };

// this is the way we like it ...

static inline int rt_msgsnd_nu(int msqid, int mtype, void *mtext, size_t msgsz, int msgflg)
{
	struct { int msqid; int mtype; void *mtext; size_t msgsz; int msgflg; int space; } arg = { msqid, mtype, mtext, msgsz, msgflg, 1 };
	return rtai_lxrt(SYSV_MSG_IDX, SIZARG, SYSV_MSGSND, &arg).i[LOW];
}

static inline ssize_t rt_msgrcv_nu(int msqid, int *mtype, void *mtext, size_t msgsz, long msgtyp, int msgflg)
{
	struct { int msqid; int *mtype; void *mtext; size_t msgsz; long msgtyp; int msgflg; int space; } arg = { msqid, mtype, mtext, msgsz, msgtyp, msgflg, 1 };
	return rtai_lxrt(SYSV_MSG_IDX, SIZARG, SYSV_MSGRCV, &arg).i[LOW];
}

// ... and this is the Unix standard, i.e. using the left hand to scrap the
// right hear.

static inline int rt_msgsnd(int msqid, struct msgbuf *msgp, size_t msgsz, int msgflg)
{
	struct { int msqid; long mtype; void *mtext; size_t msgsz; int msgflg; int space; } arg = { msqid, msgp->mtype, msgp->mtext, msgsz, msgflg, 1 };
	return rtai_lxrt(SYSV_MSG_IDX, SIZARG, SYSV_MSGSND, &arg).i[LOW];
}

static inline ssize_t rt_msgrcv(int msqid, struct msgbuf *msgp, size_t msgsz, long msgtyp, int msgflg)
{
	struct { int msqid; long *mtype; void *mtext; size_t msgsz; long msgtyp; int msgflg; int space; } arg = { msqid, &msgp->mtype, msgp->mtext, msgsz, msgtyp, msgflg, 1 };
	return rtai_lxrt(SYSV_MSG_IDX, SIZARG, SYSV_MSGRCV, &arg).i[LOW];
}

#endif /* __KERNEL__ */

// RTAI styled conditional calls comes just by nature, i.e. insuring IPC_NOWAIT.

#define rt_msgsnd_if(msqid, mtype, mtext, msgsz, msgflg)  rt_msgsnd_nu(msqid, mtype, mtext, msgsz, msgflg | IPC_NOWAIT)

#define rt_msgrcv_if(msqid, mtype, mtext, msgsz, msgtyp, msgflg)  rt_msgrcv_nu(msqid, mtype, mtext, msgsz, msgtyp, msgflg | IPC_NOWAIT)

#endif /* _RTAI_SYSV_MSG_MSG_H */
