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

#ifndef _RTAI_FUSION_TIMER_H
#define _RTAI_FUSION_TIMER_H

#include <time.h>

#include <lxrt.h>

#define TM_UNSET    -1
#define TM_ONESHOT   0

typedef struct rt_timer_info {
	RTIME period;
	RTIME date;
	RTIME tsc;
} RT_TIMER_INFO;

#ifdef __cplusplus
extern "C" {
#endif

#define rt_timer_ticks2ns(t)  (t)

#define rt_timer_ns2ticks(t)  (t)

static inline RTIME rt_timer_ns2tsc(RTIME ns)
{
	struct { RTIME ns; } arg = { ns };
	return rtai_lxrt(BIDX, SIZARG, NANO2COUNT, &arg).rt;
}

static inline RTIME rt_timer_tsc2ns(RTIME ticks)
{
	struct { RTIME ticks; } arg = { ticks };
	return rtai_lxrt(BIDX, SIZARG, COUNT2NANO, &arg).rt;
}

static inline RTIME rt_timer_read(void)
{
	struct { unsigned long dummy; } arg;
	return rtai_lxrt(BIDX, SIZARG, GET_CPU_TIME_NS, &arg).rt;
}

static inline RTIME rt_timer_tsc(void)
{
#if 1
	struct { unsigned long dummy; } arg;
	return rtai_lxrt(BIDX, SIZARG, GET_TIME, &arg).rt;
#else

#if 0  // i386
	unsigned long long t;
	__asm__ __volatile__( "rdtsc" : "=A" (t));
	return t;
#endif

#if 0  // x86_64
	unsigned int __a,__d;
	asm volatile("rdtsc" : "=a" (__a), "=d" (__d));
	return ((unsigned long)__a) | (((unsigned long)__d)<<32);
#endif

#if 0  // PPC
	unsigned long long ts;
	unsigned long chk;
	__asm__ __volatile__ ("1: mftbu %0\n"
			      "   mftb %1\n"
			      "   mftbu %2\n"
			      "   cmpw %2,%0\n"
			      "   bne 1b\n"
			      : "=r" (((unsigned long *)((void *)&ts))[0]),
				"=r" (((unsigned long *)((void *)&ts))[1]),
				"=r" (chk));
	return ts;
#endif
#endif
}

#define rt_timer_set_mode(x) \
do { \
	struct { unsigned long dummy; } arg; \
	if (!rtai_lxrt(BIDX, SIZARG, HARD_TIMER_RUNNING, &arg).i[LOW]) { \
		RT_TIMER_INFO timer_info; \
		rt_timer_inquire(&timer_info); \
		if (timer_info.period) { \
			rt_timer_start(TM_ONESHOT); \
		} \
	} \
} while (0)

static inline void rt_timer_spin(RTIME ns)
{
	struct { long ns; } arg = { ns };
	rtai_lxrt(BIDX, SIZARG, BUSY_SLEEP, &arg);
}

static inline int rt_timer_start(RTIME nstick)
{
	struct { long dummy; } arg = { 0 };
	rtai_lxrt(BIDX, SIZARG, SET_ONESHOT_MODE, &arg);
	rtai_lxrt(BIDX, SIZARG, START_TIMER, &arg);
	return 0;
}

static inline void rt_timer_stop(void)
{
	struct { unsigned long dummy; } arg;
	rtai_lxrt(BIDX, SIZARG, STOP_TIMER, &arg);
}

static inline int rt_timer_inquire(RT_TIMER_INFO *info)
{
	struct { unsigned long dummy; } arg;
	info->period = rtai_lxrt(BIDX, SIZARG, HARD_TIMER_RUNNING, &arg).i[LOW] - 1;
	info->date = info->tsc = rt_timer_tsc();
	info->date = rt_timer_tsc2ns(info->date);
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* !_RTAI_FUSION_TIMER_H */
