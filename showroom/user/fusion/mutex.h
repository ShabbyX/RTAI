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

#ifndef _RTAI_FUSION_MUTEX_H
#define _RTAI_FUSION_MUTEX_H

#include <lxrt.h>

typedef struct {
	void *mutex;
} RT_MUTEX;

#ifdef __cplusplus
extern "C" {
#endif

static inline int rt_mutex_create(RT_MUTEX *mutex, const char *name)
{
	struct { unsigned long name, icount, mode; } arg = { name ? nam2id(name): (unsigned long)name, 1, RES_SEM };
	if ((mutex->mutex = (void *)rtai_lxrt(BIDX, SIZARG, LXRT_SEM_INIT, &arg).v[LOW])) {
		rt_release_waiters(arg.name);
		return 0;
	}
	return -EINVAL;
}

static inline int rt_mutex_delete(RT_MUTEX *mutex)
{
	struct { void *mutex; } arg = { mutex->mutex };
	return rtai_lxrt(BIDX, SIZARG, LXRT_SEM_DELETE, &arg).i[LOW];
}

static inline int rt_mutex_lock(RT_MUTEX *mutex)
{
	struct { void *mutex; } arg = { mutex->mutex };
	return rtai_lxrt(BIDX, SIZARG, SEM_WAIT, &arg).i[LOW] == SEM_ERR ? -EINVAL : 0;
}

static inline int rt_mutex_unlock(RT_MUTEX *mutex)
{
	struct { void *mutex; } arg = { mutex->mutex };
	return rtai_lxrt(BIDX, SIZARG, SEM_SIGNAL, &arg).i[LOW] == SEM_ERR ? -EINVAL : 0;
}

static inline int rt_mutex_acquire(RT_MUTEX *mutex, RTIME timeout)
{
	struct { void *mutex; RTIME timeout; } arg = { mutex->mutex, timeout };
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
#define rt_mutex_release(mutex)  rt_mutex_unlock(mutex)

static inline int rt_mutex_bind(RT_MUTEX *mutex, const char *name)
{
	return rt_obj_bind(mutex, name);
}

static inline int rt_mutex_unbind (RT_MUTEX *mutex)
{
	return rt_obj_unbind(mutex);
}

static inline int rt_mutex_inquire(RT_MUTEX *mutex, void *info)
{
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* !_RTAI_FUSION_MUTEX_H */
