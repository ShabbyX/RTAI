/*
COPYRIGHT (C) 1999  Paolo Mantegazza <mantegazza@aero.polimi.it>

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

#include <linux/kernel.h>
#include <linux/module.h>

#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_sem.h>

MODULE_LICENSE("GPL");

#define STACK_SIZE 8000

#define NTASKS 8

#define EXECTIME 500000000LL

#define RR_QUANTUM 0

int SchedPolicy = RT_SCHED_RR;
RTAI_MODULE_PARM(SchedPolicy, int);
MODULE_PARM_DESC(use_rr, "Sched Policy to use (default: RR)");

static SEM sync;

static RT_TASK thread[NTASKS];

static int nsw[NTASKS];

static RTIME endtime;

static void fun(long indx)
{
	rt_printk("TASK %d ENTERED.\n", indx + 1);
	rt_sem_wait(&sync);
	rt_printk("TASK %d STARTS WORKING %s.\n", indx + 1, SchedPolicy ? "RR" : "FIFO");
	while(rt_get_cpu_time_ns() < endtime) nsw[indx]++;
	rt_printk("TASK %d EXITS.\n", indx + 1);
}

int init_module(void)
{
	int i;

	rt_sem_init(&sync, 0);
	start_rt_timer(0);
	for (i = 0; i < NTASKS; i++) {
		rt_task_init_cpuid(&thread[i], fun, i, STACK_SIZE, 0, 0, 0, 0);
		rt_set_sched_policy(&thread[i], SchedPolicy, RR_QUANTUM);
	}
	for (i = 0; i < NTASKS; i++) {
		rt_task_resume(&thread[i]);
		while(!(thread[i].state & RT_SCHED_SEMAPHORE));
	}
	endtime = rt_get_cpu_time_ns() + EXECTIME;
	rt_sem_broadcast(&sync);
	return 0;
}

void cleanup_module(void)
{
	int i;

	stop_rt_timer();
	rt_sem_delete(&sync);
	rt_printk("EXECUTION COUNTERS:\n");
	for (i = 0; i < NTASKS; i++) {
		printk("TASK %d -> %d\n", i + 1, nsw[i]);
		rt_task_delete(&thread[i]);
	}
}
