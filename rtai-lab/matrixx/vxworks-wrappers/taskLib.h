/*
COPYRIGHT (C) 2001-2006  Paolo Mantegazza  (mantegazza@aero.polimi.it)
			    Giuseppe Quaranta (quaranta@aero.polimi.it)

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

#ifndef _VXW_TASKLIB_H_
#define _VXW_TASKLIB_H_

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/io.h>
#include <sys/poll.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>

#include <rtai_lxrt.h>

#include "semLib.h"

#define VX_FP_TASK  1

#define NAME_SIZE 30

struct thread_args {
	char name[NAME_SIZE];
	void *(*fun)(long, ...);
	long a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, prio, stksiz, fpu;
	pthread_t thread;
	RT_TASK *task;
	SEM_ID sem;
};

#ifndef __SUPPORT_FUN__
#define __SUPPORT_FUN__

static void *support_fun(struct thread_args *args)
{
//	struct thread_args *args = ((struct thread_args *)arg);

 	if (!(args->task = rt_task_init_schmod(nam2num(args->name), args->prio, 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT SUPPORT TASK FOR %s.\n", args->name);
		free(args);
		return (void *)1;
	}
	iopl(3);
	if (args->fpu == VX_FP_TASK) {
	 	rt_task_use_fpu(args->task, 1);
	}
	rt_set_usp_flags_mask(FORCE_SOFT);
	rt_grow_and_lock_stack(args->stksiz - 10000);
	rt_make_hard_real_time();
	semGive(args->sem);
	args->fun(args->a1, args->a2, args->a3, args->a4, args->a5, args->a6, args->a7, args->a8, args->a9, args->a10);
	rt_make_soft_real_time();
	rt_task_delete(args->task);
	return NULL;
}

#endif

#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN  (64*1024)
#endif

static inline long taskSpawn(char *name, long priority, long options, long stackSize, FUNCPTR entryPt, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6, long arg7, long arg8, long arg9, long arg10)
{
	struct thread_args *args;
	 pthread_attr_t attr;

	args = malloc(sizeof(struct thread_args));
	strncpy(args->name, name, NAME_SIZE - 1);
	args->name[NAME_SIZE - 1] = 0;
	args->fun    = (void *)entryPt;
	args->a1     = arg1;
	args->a2     = arg2;
	args->a3     = arg3;
	args->a4     = arg4;
	args->a5     = arg5;
	args->a6     = arg6;
	args->a7     = arg7;
	args->a8     = arg8;
	args->a9     = arg9;
	args->a10    = arg10;
	args->prio   = priority;
	args->fpu    = options == VX_FP_TASK ? 1 : 0;
	args->stksiz = stackSize;
	args->task   = NULL;
	 pthread_attr_init(&attr);
	 if (pthread_attr_setstacksize(&attr, stackSize > PTHREAD_STACK_MIN ? stackSize : PTHREAD_STACK_MIN)) {
		free(args);
		  return ERROR;
	 }
	args->sem = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
	 if (pthread_create(&args->thread, &attr, (void *(*)(void *))support_fun, args)) {
		free(args);
		  return ERROR;
	 }
	semTake(args->sem, WAIT_FOREVER);
	semDelete(args->sem);
	return (long)args;
}

static inline int taskDelete(long tid)
{
	struct thread_args *args = (struct thread_args *)tid;

	if (tid == 0) {
		return OK;
	}

//	while(rt_is_hard_real_time(args->task)) {
		//poll(0, 0, 10);
//	}
	rt_task_suspend(args->task);
	pthread_cancel(args->thread);
	pthread_join(args->thread, NULL);
	free(args);
	return OK;
}

static inline int taskSuspend(long tid)
{
	struct thread_args *args = (struct thread_args *)tid;

	return !rt_task_suspend(args->task) ? OK : ERROR;
}

static inline long taskIdSelf(void)
{
	return (long)rt_buddy();
}

static inline long taskIdVerify(long tid)
{
	struct thread_args *args = (struct thread_args *)tid;
	return tid && args->task ? OK : ERROR;
}

static inline int taskDelay(int ticks)
{
	return !rt_sleep(ticks) ? OK : ERROR;
}

static inline int taskRestart(long tid)
{
	printf("*** taskRestart not available yet ***\n");
	return ERROR;
}

#endif
