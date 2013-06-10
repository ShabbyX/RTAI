/*
 * Copyright (C) 2005 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef _RTAI_FUSION_QUEUE_H
#define _RTAI_FUSION_QUEUE_H

#include <lxrt.h>

#define MSGQ_INIT            TBX_INIT
#define MSGQ_DELETE          TBX_DELETE
#define NAMED_MSGQ_INIT      NAMED_TBX_INIT
#define NAMED_MSGQ_DELETE    NAMED_TBX_DELETE
#define MSG_SEND             TBX_SEND
#define MSG_SEND_IF          TBX_SEND_IF
#define MSG_SEND_UNTIL       TBX_SEND_UNTIL
#define MSG_SEND_TIMED       TBX_SEND_TIMED
#define MSG_RECEIVE          TBX_RECEIVE
#define MSG_RECEIVE_IF       TBX_RECEIVE_IF
#define MSG_RECEIVE_UNTIL    TBX_RECEIVE_UNTIL
#define MSG_RECEIVE_TIMED    TBX_RECEIVE_TIMED
#define MSG_BROADCAST        TBX_BROADCAST

#define Q_PRIO   PRIO_Q
#define Q_FIFO   FIFO_Q
#define Q_DMA    0x100
#define Q_SHARED 0x200

#define Q_UNLIMITED  (0)

#define Q_URGENT     0x0
#define Q_NORMAL     0x1
#define Q_BROADCAST  0x2

typedef struct rt_queue_placeholder {
	void *msgq;
	void *heap;
} RT_QUEUE;

#ifdef __cplusplus
extern "C" {
#endif

#define GLOBAL_HEAP_ID  0x9ac6d9e7  // nam2num("RTGLBH");
#define RTAI_SHM_DEV    "/dev/rtai_shm"

#include <fcntl.h>

static inline void *rt_global_heap_open(void)
{
	int hook, size;
	void *adr;
	if ((hook = open(RTAI_SHM_DEV, O_RDWR)) <= 0) {
		return NULL;
	} else {
		struct { unsigned long name, arg, suprt; } arg = { GLOBAL_HEAP_ID, 0, 0 };
		if ((size = rtai_lxrt(BIDX, SIZARG, SHM_ALLOC, &arg).i[LOW])) {
			if ((adr = mmap(0, size, PROT_WRITE | PROT_READ, MAP_SHARED | MAP_LOCKED, hook, 0)) == (void *)-1) {;
				rtai_lxrt(BIDX, sizeof(unsigned long), SHM_FREE, &arg.name);
				adr = 0;
			}
			arg.arg = (unsigned long)adr;
			rtai_lxrt(BIDX, SIZARG, HEAP_SET, &arg);
		} else {
			adr = 0;
		}
	}
	close(hook);
	return adr;
}

static inline void rt_global_heap_close(void)
{
	int hook, size;
	unsigned long name = GLOBAL_HEAP_ID;
	struct { void *nameadr; } arg = { &name };
	if ((hook = open(RTAI_SHM_DEV, O_RDWR)) > 0) {
		if ((size = rtai_lxrt(BIDX, SIZARG, SHM_SIZE, &arg).i[LOW])) {
			munmap((void *)name, size);
		}
		close(hook);
	}
}

static inline int rt_queue_create(RT_QUEUE *q, const char *name, size_t poolsize, size_t qlimit, int mode)
{
	struct { unsigned long msgq; long nmsg; long msg_size; } arg = { name ? nam2id(name) : (unsigned long)name, qlimit != Q_UNLIMITED ? qlimit : 10000, sizeof(void *) };
        RT_TASK me;
        if (!(me.task = rtai_tskext())) {
                me.task = rt_task_ext((unsigned long)name, 0, 0xF);
        }
	if ((q->msgq = rtai_lxrt(BIDX, SIZARG, NAMED_MSGQ_INIT, &arg).v[LOW])) {
		q->heap = rt_global_heap_open();
		rt_release_waiters(arg.msgq);
		return 0;
	}
	return -ENOMEM;
}

static inline int rt_queue_delete(RT_QUEUE *q)
{
	struct { void *msgq; } arg = { q->msgq };
	rt_global_heap_close();
	return rtai_lxrt(BIDX, SIZARG, NAMED_MSGQ_DELETE, &arg).i[LOW] ? -EINVAL : 0;
}

static inline void *rt_queue_alloc(RT_QUEUE *q, size_t size)
{
	void *buf;
	if ((buf = rt_malloc(size + sizeof(void *)))) {
		return (buf + sizeof(void *));
	}
	return 0;
}

static inline int rt_queue_free(RT_QUEUE *q, void *buf)
{
	if (buf) {
		buf -= sizeof(void *);
	} else {
		return -EINVAL;
	}
	rt_free(buf);
	return 0;
}

static inline int rt_queue_send(RT_QUEUE *q, void *buf, size_t size, int mode)
{
	void *bufp = buf - sizeof(void *) - (unsigned long)q->heap;
	struct { void *msgq; void *msg; long msg_size; long msgprio; long space; } arg = { q->msgq, &bufp, sizeof(void *), mode & 1, 0 };
	((int *)buf)[-1] = size;
	if (mode & Q_BROADCAST) {
	        return rtai_lxrt(BIDX, SIZARG, MSG_BROADCAST, &arg).i[LOW];
	} else {
	        return rtai_lxrt(BIDX, SIZARG, MSG_SEND_IF, &arg).i[LOW];
	}
return 0;
}

static inline ssize_t rt_queue_recv(RT_QUEUE *q, void **buf, RTIME timeout)
{
	int size;
	if (timeout == TM_INFINITE) {
        	struct { void *msgq; void *msg; long msg_size; long *msgprio; long space; } arg = { q->msgq, buf, sizeof(void *), &arg.space, 0 };
		if ((size = rtai_lxrt(BIDX, SIZARG, MSG_RECEIVE, &arg).i[LOW])) {
			size = -EINVAL;
		}
	} else if (timeout == TM_NONBLOCK) {
        	struct { void *msgq; void *msg; long msg_size; long *msgprio; long space; } arg = { q->msgq, buf, sizeof(void *), &arg.space, 0 };
		if ((size = rtai_lxrt(BIDX, SIZARG, MSG_RECEIVE_IF, &arg).i[LOW])) {
			size = -EWOULDBLOCK;
		}
	} else {
        	struct { void *msgq; void *msg; long msg_size; long *msgprio; RTIME until; long space; } arg = { q->msgq, buf, sizeof(void *), &arg.space, timeout, 0 };
		if ((size = rtai_lxrt(BIDX, SIZARG, MSG_RECEIVE_TIMED, &arg).i[LOW])) {
			size = -ETIMEDOUT;
		}
	}
	if (size) {
		return size;
	}
	buf[0] += (unsigned long)(q->heap + sizeof(void *));
	return ((int *)buf[0])[-1];
}

static inline int rt_queue_bind(RT_QUEUE *q, const char *name)
{
	int retval;
	q->heap = rt_global_heap_open();
	retval = rt_obj_bind(q, name);
	return retval;
}

static inline int rt_queue_unbind(RT_QUEUE *q)
{
	rt_global_heap_close();
	return rt_obj_unbind(q);
}

static inline int rt_queue_inquire(RT_QUEUE *q, void *info)
{
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* !_RTAI_FUSION_QUEUE_H */
