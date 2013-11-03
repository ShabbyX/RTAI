/*
 *
 * Copyright 2005 Paolo Mantegazza (mantegazza@aero.polimi.it)
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
 *
 */

/* REMINDER 1: timer types and functions to use
types:
FTIME
FTIMER_INFO
functions:
FTIME ftimer_ns2ticks(FTIME ns)
FTIME ftimer_ticks2ns(FTIME ns)
FTIME ftimer_ns2tsc(FTIME ns)
FTIME ftimer_tsc2ns(FTIME ns)
int ftimer_inquire(FTIMER_INFO *info)
FTIME ftimer_read(void)
FTIME ftimer_tsc(void)
int ftimer_start(FTIME nstick)
void ftimer_stop(void)
*/

/* REMINDER 2: tasks types and functions to use
types:
FTASK
FTASK_INFO
functions:
int ftask_create(FTASK *task, const char *name, int stksize, int prio, int mode)
int ftask_start(FTASK *task, void (*fun)(void *cookie), void *cookie)
int ftask_spawn(FTASK *task, const char *name, int stksize, int prio, int mode, void (*entry)(void *cookie), void *cookie)
int ftask_shadow(FTASK *task, const char *name, int prio, int mode)
int ftask_set_priority(FTASK *task, int prio)
int ftask_bind(FTASK *task, const char *name, FTIME timeout)
int ftask_unbind(FTASK *task)
int ftask_sleep(FTIME delay)
int ftask_sleep_until(FTIME until)
int ftask_inquire(FTASK *task, FTASK_INFO *info)
FTASK *ftask_self(void)
void ftask_make_soft_real_time(void)
void ftask_make_hard_real_time(void)
*/

#ifndef _RTAI_FUSION_H
#define _RTAI_FUSION_H

#include <nucleus/timer.h>
#include <nucleus/thread.h>

#define TM_UNSET    XN_NO_TICK
#define TM_ONESHOT  XN_APERIODIC_TICK

typedef long long FTIME;

typedef struct ftimer_info
{ FTIME period; FTIME date; FTIME tsc; } FTIMER_INFO;

#ifdef __cplusplus
extern "C" {
#endif

FTIME rt_timer_ns2ticks(FTIME ns);
static inline FTIME ftimer_ns2ticks(FTIME ns)
{
	return rt_timer_ns2ticks(ns);
}

FTIME rt_timer_ticks2ns(FTIME ticks);
static inline FTIME ftimer_ticks2ns(FTIME ticks)
{
	return rt_timer_ticks2ns(ticks);
}

FTIME rt_timer_ns2tsc(FTIME ns);
static inline FTIME ftimer_ns2tsc(FTIME ns)
{
	return rt_timer_ns2tsc(ns);
}

FTIME rt_timer_tsc2ns(FTIME ticks);
static inline FTIME ftimer_tsc2ns(FTIME ns)
{
	return rt_timer_tsc2ns(ns);
}

int rt_timer_inquire(FTIMER_INFO *info);
static inline int ftimer_inquire(FTIMER_INFO *info)
{
	return rt_timer_inquire(info);
}

FTIME rt_timer_read(void);
static inline FTIME ftimer_read(void)
{
	return rt_timer_read();
}

FTIME rt_timer_tsc(void);
static inline FTIME ftimer_tsc(void)
{
	return rt_timer_tsc();
}

int rt_timer_start(FTIME nstick);
static inline int ftimer_start(FTIME nstick)
{
	return rt_timer_start(nstick);
}

void rt_timer_stop(void);
static inline void ftimer_stop(void)
{
	return ftimer_stop();
}

#ifdef __cplusplus
}
#endif

#define T_FPU     XNFPU
#define T_SUSP    XNSUSP
/* <!> High bits must not conflict with XNFPU|XNSHADOW|XNSHIELD|XNSUSP. */
#define T_CPU(cpu) (1 << (24 + (cpu & 7))) /* Up to 8 cpus [0-7] */

#define T_LOPRIO  1
#define T_HIPRIO  99

typedef struct ftask_placeholder
{ unsigned long opaque, opaque2; } FTASK_PLACEHOLDER;

typedef struct ftask_info
{
	int bprio;			/* !< Base priority. */
	int cprio;			/* !< Current priority. */
	unsigned status;		/* !< Status. */
	FTIME relpoint;		/* !< Periodic release point. */
	char name[XNOBJECT_NAME_LEN]; /* !< Symbolic name. */
} FTASK_INFO;

typedef FTASK_PLACEHOLDER FTASK;

#ifdef __cplusplus
extern "C" {
#endif

#define FPRIO ((prio = T_HIPRIO - prio) < T_LOPRIO ? 1 : prio)

int rt_task_create(FTASK *task, const char *name, int stksize, int prio, int mode);
static inline int ftask_create(FTASK *task, const char *name, int stksize, int prio, int mode)
{
	return rt_task_create(task, name, stksize, FPRIO, mode);
}

int rt_task_start(FTASK *task, void (*fun)(void *cookie), void *cookie);
static inline int ftask_start(FTASK *task, void (*fun)(void *cookie), void *cookie)
{
	return rt_task_start(task, fun, cookie);
}

static inline int ftask_spawn(FTASK *task, const char *name, int stksize, int prio, int mode, void (*entry)(void *cookie), void *cookie)
{
	int err = ftask_create(task, name, stksize, prio, mode);
	return !err ? ftask_start(task, entry, cookie) : err;
}

int rt_task_shadow(FTASK *task, const char *name, int prio, int mode);
static inline int ftask_shadow(FTASK *task, const char *name, int prio, int mode)
{
	return rt_task_shadow(task, name, FPRIO, mode);
}

int rt_task_set_priority(FTASK *task, int prio);
static inline int ftask_set_priority(FTASK *task, int prio)
{
	return rt_task_set_priority(task, FPRIO);
}

int rt_task_bind(FTASK *task, const char *name, FTIME timeout);
static inline int ftask_bind(FTASK *task, const char *name, FTIME timeout)
{
	return rt_task_bind(task, name, timeout);
}

static inline int ftask_unbind(FTASK *task)
{
	task->opaque = 0UL;
	return 0;
}

int rt_task_sleep(FTIME delay);
static inline int ftask_sleep(FTIME delay)
{
	return rt_task_sleep(delay);
}

int rt_task_sleep_until(FTIME date);
static inline int ftask_sleep_until(FTIME until)
{
	return rt_task_sleep_until(until);
}

int rt_task_inquire(FTASK *task, FTASK_INFO *info);
static inline int ftask_inquire(FTASK *task, FTASK_INFO *info)
{
	return rt_task_inquire(task, info);
}

FTASK *rt_task_self(void);
static inline FTASK *ftask_self(void)
{
	return rt_task_self();
}


static inline void ftask_make_soft_real_time(void)
{
#include <sys/poll.h>
	poll(0, 0, 0);
}

static inline void ftask_make_hard_real_time(void)
{
	rt_task_sleep(1);
}

#ifdef __cplusplus
}
#endif

#endif /* !_RTAI_FUSION_H */
