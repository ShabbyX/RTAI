/*
COPYRIGHT (C) 2001  Paolo Mantegazza <mantegazza@aero.polimi.it>

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

/* An adaption of the same test for posix mutexes, by Steve Papacharalambous */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>

#include <asm/rtai.h>
#include <rtai_sched.h>
#include <rtai_sem.h>
#include <rtai_msg.h>

#define RT_STACK 2000

static RT_TASK taskl, taskm, taskh;

static SEM mutex;

static int SemType = 1;
RTAI_MODULE_PARM(SemType, int);

//#define MUTEX_LOCK(mutex)  rt_sem_wait((mutex))
#define MUTEX_LOCK(mutex)  rt_sem_wait_timed((mutex), 2000000000)

void taskl_func(long tid)
{
	unsigned int msg = 0;
	rt_receive(0, &msg);
	rt_receive(0, &msg);
	while (rt_sem_wait(&mutex) <= 1) {
		if (SemType) {
			rt_sem_wait(&mutex);
			rt_busy_sleep(100000);
			rt_sem_wait(&mutex);
		}
		rt_send(&taskh, msg); // cause the highest task to run
		rt_busy_sleep(100000);
		if (SemType) {
			rt_sem_signal(&mutex);
			rt_busy_sleep(100000);
			rt_sem_signal(&mutex);
		}
		rt_sem_signal(&mutex);
		rt_sleep(nano2count(500000000));
	}
	rt_task_suspend(0);
}

void taskm_func(long tid)
{
	unsigned int msg = 0;
	rt_receive(0, &msg);
	rt_send(&taskl, msg);
	while (1) {
		rt_receive(&taskh, &msg);
		rt_busy_sleep(5000000);
	}
}

void taskh_func(long tid)
{
	RTIME time;
	unsigned int msg = 0, wait;
	rt_send(&taskm, msg);
	rt_send(&taskl, msg);
	while (1) {
		rt_receive(&taskl, &msg);
		rt_send(&taskm, msg);
		time = rt_get_time_ns();
		if (rt_sem_wait(&mutex) <= 1) {
			if ((wait = (int)(rt_get_time_ns() - time)) > 250000) {
				rt_printk("PRIORITY INVERSION, WAITED FOR %d us\n", wait/1000);
			} else {
				rt_printk("NO PRIORITY INVERSION, WAITED FOR %d us\n", wait/1000);
			}
			if (SemType) {
				rt_sem_wait(&mutex);
				rt_sem_wait(&mutex);
				rt_busy_sleep(100000);
				rt_sem_wait(&mutex);
			}
			rt_busy_sleep(100000);
			if (SemType) {
				rt_sem_signal(&mutex);
				rt_busy_sleep(100000);
				rt_sem_signal(&mutex);
				rt_sem_signal(&mutex);
			}
			rt_sem_signal(&mutex);
		} else {
			rt_task_suspend(0);
		}
	}
}

int init_module(void)
{
	rt_set_oneshot_mode();
	start_rt_timer(0);
	if (SemType) {
		printk("USING A RESOURCE SEMAPHORE, AND WE HAVE ...\n");
		rt_typed_sem_init(&mutex, 1, RES_SEM);
	} else {
		printk("USING A BINARY SEMAPHORE, AND WE HAVE ...\n");
		rt_typed_sem_init(&mutex, 1, BIN_SEM | PRIO_Q);
	}
	rt_task_init_cpuid(&taskl, taskl_func, 0, RT_STACK, 1000, 0, 0, 0);
	rt_task_init_cpuid(&taskm, taskm_func, 0, RT_STACK,  500, 0, 0, 0);
	rt_task_init_cpuid(&taskh, taskh_func, 0, RT_STACK,    0, 0, 0, 0);
	rt_task_resume(&taskl);
	rt_task_resume(&taskm);
	rt_task_resume(&taskh);
	return 0;
}

void cleanup_module(void)
{
	stop_rt_timer();
	rt_sem_delete(&mutex);
	rt_task_delete(&taskl);
	rt_task_delete(&taskm);
	rt_task_delete(&taskh);
}
