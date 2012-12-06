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


#ifndef _RTAI_LXRT_XN_H
#define _RTAI_LXRT_XN_H

#define WAIT_PERIOD	 		 4
#define START_TIMER	 		 7
#define STOP_TIMER	 		 8
#define GET_TIME	 		 9
#define COUNT2NANO			10
#define NANO2COUNT			11
#define SET_ONESHOT_MODE		14
#define GET_CPU_TIME_NS			20
#define MAKE_PERIODIC_NS 		30
#define REQUEST_RTC                    214
#define RELEASE_RTC                    215


#define LXRT_TASK_INIT          1002
#define LXRT_TASK_DELETE        1003
#define RT_BUDDY                1012

#define LOW 0

struct rt_task_struct;

// Keep LXRT call enc/decoding together, so you are sure to act consistently.
// This is the encoding, note " | GT_NR_SYSCALLS" to ensure not a Linux syscall, ...
#define GT_NR_SYSCALLS  (1 << 11)
#define ENCODE_LXRT_REQ(dynx, srq, lsize)  (((dynx) << 24) | ((srq) << 12) | GT_NR_SYSCALLS | (lsize))

#define RTAI_SYS_VECTOR        0xf6

#define __rtai_stringize0(_s_) #_s_
#define __rtai_stringize(_s_)  __rtai_stringize0(_s_)
#define __rtai_trap_call(_t_)  _t_
#define __rtai_do_trap0(_t_)   __rtai_stringize(int $ _t_)
#define __rtai_do_trap(_t_)    __rtai_do_trap0(__rtai_trap_call(_t_))

#define RTAI_DO_TRAP(v, r, a1, a2)  do { __asm__ __volatile__ ( __rtai_do_trap(v): : "a" (a1), "c" (a2), "d" (&r)); } while (0)

union rtai_lxrt_t {
    RTIME rt;
    int i[2];
    void *v[2];
};

static union rtai_lxrt_t _rtai_lxrt(int srq, void *arg)
{
	union rtai_lxrt_t retval;
	RTAI_DO_TRAP(RTAI_SYS_VECTOR, retval, srq, arg);
	return retval;
}

static inline union rtai_lxrt_t rtai_lxrt(short int dynx, short int lsize, int srq, void *arg)
{
	return _rtai_lxrt(ENCODE_LXRT_REQ(dynx, srq, lsize), arg);
}

#define rtai_iopl()  do { extern int iopl(int); iopl(3); } while (0)

#define BIDX    0
#define SIZARG  sizeof(arg)

static inline unsigned long nam2num(const char *name)
{
        unsigned long retval = 0;
	int c, i;

	for (i = 0; i < 6; i++) {
		if (!(c = name[i]))
			break;
		if (c >= 'a' && c <= 'z') {
			c +=  (11 - 'a');
		} else if (c >= 'A' && c <= 'Z') {
			c += (11 - 'A');
		} else if (c >= '0' && c <= '9') {
			c -= ('0' - 1);
		} else {
			c = c == '_' ? 37 : 38;
		}
		retval = retval*39 + c;
	}
	if (i > 0)
		return retval;
	else
		return 0xFFFFFFFF;
}

static inline void *rtai_tskext(void)
{
        struct { unsigned long dummy; } arg;
        return (void *)rtai_lxrt(BIDX, SIZARG, RT_BUDDY, &arg).v[LOW];
}

static inline RT_TASK *ftask_init(unsigned long name, int priority)
{
	struct { unsigned long name; int priority, stack_size, max_msg_size, cpus_allowed; } arg = { name, priority, 0, 0, 0 };
	return (RT_TASK *)rtai_lxrt(BIDX, SIZARG, LXRT_TASK_INIT, &arg).v[LOW];
}

static inline int rtai_task_delete(RT_TASK *task)
{
	struct { void *task; } arg = { task };
        return rtai_lxrt(BIDX, SIZARG, LXRT_TASK_DELETE, &arg).i[LOW];
}

static inline int rtai_task_make_periodic_relative_ns(RT_TASK *task, RTIME start_delay, RTIME period)
{
	struct { RT_TASK *task; RTIME start_time, period; } arg = { task, start_delay, period };
	return rtai_lxrt(BIDX, SIZARG, MAKE_PERIODIC_NS, &arg).i[LOW];
}

static inline int rtai_task_wait_period(void)
{
	struct { unsigned long dummy; } arg;
	return rtai_lxrt(BIDX, SIZARG, WAIT_PERIOD, &arg).i[LOW];
}

static inline RTIME nano2count(RTIME nanos)
{
        struct { RTIME nanos; } arg = { nanos };
        return rtai_lxrt(BIDX, SIZARG, NANO2COUNT, &arg).rt;
}

static inline RTIME count2nano(RTIME count)
{
        struct { RTIME count; } arg = { count };
        return rtai_lxrt(BIDX, SIZARG, COUNT2NANO, &arg).rt;
}

static inline RTIME rtai_get_cpu_time_ns(void)
{
	struct { unsigned long dummy; } arg;
	return rtai_lxrt(BIDX, SIZARG, GET_CPU_TIME_NS, &arg).rt;
}

static inline RTIME rtai_get_time(void)
{
	struct { unsigned long dummy; } arg;
	return rtai_lxrt(BIDX, SIZARG, GET_TIME, &arg).rt;
}

static inline int start_rtai_timer(void)
{
        struct { int dummy; } arg;
	rtai_lxrt(BIDX, SIZARG, SET_ONESHOT_MODE, &arg);
	rtai_lxrt(BIDX, SIZARG, START_TIMER, &arg);
	return 0;
}

static inline void stop_rtai_timer(void)
{
	struct { unsigned long dummy; } arg;
	rtai_lxrt(BIDX, SIZARG, STOP_TIMER, &arg);
}

static inline void rt_request_rtc(int rtc_freq, void *handler)
{
	struct { long rtc_freq; void *handler; } arg = { rtc_freq, handler };
	return rtai_lxrt(BIDX, SIZARG, REQUEST_RTC, &arg);
}

static inline void rt_release_rtc(void)
{
	struct { unsigned long dummy; } arg;
	rtai_lxrt(BIDX, SIZARG, RELEASE_RTC, &arg);
}

#include <pthread.h>

#define RT_THREAD_STACK_MIN  64*1024

static inline int rtai_thread_create(void *fun, void *args, int stack_size)
{
        pthread_t thread;
        pthread_attr_t attr;

        pthread_attr_init(&attr);
        if (pthread_attr_setstacksize(&attr, stack_size > RT_THREAD_STACK_MIN ?
stack_size : RT_THREAD_STACK_MIN)) {
                return -1;
        }
        if (pthread_create(&thread, &attr, (void *(*)(void *))fun, args)) {
                return -1;
        }
        return thread;
}

#endif /* !_RTAI_LXRT_XN_H */
