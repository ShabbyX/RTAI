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


#ifndef _RTAI_FUSION_COND_H
#define _RTAI_FUSION_COND_H

#include <mutex.h>

typedef struct {
	void *cond;
} RT_COND;

#ifdef __cplusplus
extern "C" {
#endif

static inline int rt_cond_create(RT_COND *cond, const char *name)
{
	struct { unsigned long name, icount, mode; } arg = { name ? nam2id(name) : (unsigned long)name, 0, RES_SEM };
        if ((cond->cond = (void *)rtai_lxrt(BIDX, SIZARG, LXRT_SEM_INIT, &arg).v[LOW])) {
                rt_release_waiters(arg.name);
                return 0;
        }
        return -EINVAL;
}

static inline int rt_cond_delete(RT_COND *cond)
{
        struct { void *cond; } arg = { cond->cond };
	return rtai_lxrt(BIDX, SIZARG, LXRT_SEM_DELETE, &arg).i[LOW];
}

static inline int rt_cond_wait(RT_COND *cond, RT_MUTEX *mutex, RTIME timeout)
{
	struct { void *cond, *mutex; RTIME time; } arg = { cond->cond, mutex->mutex, timeout };
	int retval;
	if (timeout == TM_INFINITE) {
		return rtai_lxrt(BIDX, SIZARG, COND_WAIT, &arg).i[LOW] == SEM_ERR ? -EINVAL : 0;
	} else if (timeout == TM_NONBLOCK) {
		return -EWOULDBLOCK;
	}
	retval = rtai_lxrt(BIDX, SIZARG, COND_WAIT_TIMED, &arg).i[LOW];
	return retval == SEM_TIMOUT ? -ETIMEDOUT : retval == SEM_ERR ? -EINVAL : 0;
}

static inline int rt_cond_signal(RT_COND *cond)
{
        struct { void *cond; } arg = { cond->cond };
        return rtai_lxrt(BIDX, SIZARG, COND_SIGNAL, &arg).i[LOW] == SEM_ERR ? -EINVAL : 0;
}

static inline int rt_cond_broadcast(RT_COND *cond)
{
        struct { void *cond; } arg = { cond->cond };
        return rtai_lxrt(BIDX, SIZARG, SEM_BROADCAST, &arg).i[LOW] == SEM_ERR ? -EINVAL : 0;
}

static inline int rt_cond_bind(RT_COND *cond, const char *name)
{
        return rt_obj_bind(cond, name);
}

static inline int rt_cond_unbind (RT_COND *cond)
{
        return rt_obj_unbind(cond);
}

static inline int rt_cond_inquire(RT_COND *cond, void *info)
{
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* !_RTAI_FUSION_COND_H */
