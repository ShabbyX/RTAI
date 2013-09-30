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

#ifndef _RTAI_FUSION_SEM_H
#define _RTAI_FUSION_SEM_H

#include <lxrt.h>

#define S_PRIO  PRIO_Q
#define S_FIFO  FIFO_Q

typedef struct {
	void *sem;
} RT_SEM;

#ifdef __cplusplus
extern "C" {
#endif

static inline int rt_sem_create(RT_SEM *sem, const char *name, unsigned long icount, int mode)
{
	struct { unsigned long name, icount, mode; } arg = { name ? nam2id(name) : (unsigned long)name, icount, CNT_SEM | mode };
	if ((sem->sem = (void *)rtai_lxrt(BIDX, SIZARG, LXRT_SEM_INIT, &arg).v[LOW])) {
		rt_release_waiters(arg.name);
		return 0;
	}
	return -EINVAL;
}

static inline int rt_sem_delete(RT_SEM *sem)
{
	struct { void *sem; } arg = { sem->sem };
	return rtai_lxrt(BIDX, SIZARG, LXRT_SEM_DELETE, &arg).i[LOW];
}

static inline int rt_sem_p(RT_SEM *sem, RTIME timeout)
{
	struct { void *sem; RTIME timeout; } arg = { sem->sem, timeout };
	int retval;
	if (timeout == TM_INFINITE) {
		return rtai_lxrt(BIDX, SIZARG, SEM_WAIT, &arg).i[LOW] == SEM_ERR ? -EINVAL : 0;
	} else if (timeout == TM_NONBLOCK) {
		retval = rtai_lxrt(BIDX, SIZARG, SEM_WAIT_IF, &arg).i[LOW];
		return retval <= 0 ? -EWOULDBLOCK : retval == SEM_ERR ? -EINVAL : 0;
	}
	retval = rtai_lxrt(BIDX, SIZARG, SEM_WAIT_TIMED, &arg).i[LOW];
	return retval == SEM_TIMOUT ? -ETIMEDOUT : retval == SEM_ERR ? -EINVAL : 0;
}

static inline int rt_sem_v(RT_SEM *sem)
{
	struct { void *sem; } arg = { sem->sem };
	return rtai_lxrt(BIDX, SIZARG, SEM_SIGNAL, &arg).i[LOW] == SEM_ERR ? -EINVAL : 0;
}

static inline int rt_sem_broadcast(RT_SEM *sem)
{
	struct { void *sem; } arg = { sem->sem };
	return rtai_lxrt(BIDX, SIZARG, SEM_BROADCAST, &arg).i[LOW] == SEM_ERR ? -EINVAL : 0;
}

static inline int rt_sem_bind(RT_SEM *sem, const char *name)
{
	return rt_obj_bind(sem, name);
}

static inline int rt_sem_unbind (RT_SEM *sem)
{
	return rt_obj_unbind(sem);
}

static inline int rt_sem_inquire(RT_SEM *sem, void *info)
{
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* !_RTAI_FUSION_SEM_H */
