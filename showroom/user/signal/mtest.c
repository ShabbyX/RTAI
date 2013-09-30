/*
 * Copyright (C) 2006 Paolo Mantegazza <mantegazza@aero.polimi.it>
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

#include <linux/kernel.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");

#include <rtai_sem.h>
#include <rtai_msg.h>
#include <rtai_malloc.h>
#include "rtai_signal.h"

#define NTASKS       4
#define SMLTN        5
#define CPUMAP       1

#define MASTER_SIGNAL  0
#define RESUME_SIGNAL  0
#define END_SIGNAL     1

#define NLOOP     1000
#define PERIOD    1000000
#define WAISTIME  PERIOD/(2*NTASKS)
#define STKSZ     4000

static RT_TASK mastertask, task[NTASKS];
static volatile int endmaster = NLOOP, endtask[NTASKS];

static SEM barrier;

static void master_sighdl(long signal, RT_TASK *sched_task)
{
	static int taskidx;
	int i;
	for (i = 0; i < SMLTN; i++) {
		rt_trigger_signal(RESUME_SIGNAL, &task[taskidx++%NTASKS]);
	}
}

static void resume_sighdl(long signal, RT_TASK *task)
{
	rt_task_resume(task);
}

static void end_sighdl(long signal, RT_TASK *task2end)
{
	int i;
	for (i = 0; i < NTASKS; i++) {
		if (&task[i] == task2end) {
			endtask[i] = 1;
			rt_task_resume(task2end);
			return;
		}
	}
}

static void task_fun(long taskidx)
{
	int loop;
	float f;
	rt_request_signal(RESUME_SIGNAL, resume_sighdl);
	rt_request_signal(END_SIGNAL, end_sighdl);
	loop = 0;
	f = 0.0;
	rt_sem_wait_barrier(&barrier);
	while (!endtask[taskidx]) {
		rt_task_suspend(&task[taskidx]);
		++f;
		rt_printk("Task %d executing loop # %d\n", taskidx + 1, ++loop);
		rt_busy_sleep(nano2count(WAISTIME));
	}
	rt_release_signal(RESUME_SIGNAL, 0);
	rt_release_signal(END_SIGNAL, 0);
	rt_return(rt_receive(0, &loop), 0);
	rt_sem_wait_barrier(&barrier);
	rt_printk("TASK %d ENDs\n", taskidx + 1);
	rt_task_suspend(0);
}

static void master_fun(long arg)
{
	int i, ret;
	rt_request_signal(MASTER_SIGNAL, master_sighdl);
	rt_sem_wait_barrier(&barrier);
	while (--endmaster) {
		rt_sleep(nano2count(PERIOD));
		rt_trigger_signal(MASTER_SIGNAL, 0);
	}
	for (i = 0; i < NTASKS; i++) {
		rt_trigger_signal(END_SIGNAL, &task[i]);
		rt_rpc(&task[i], 0, &ret);
	}
	rt_release_signal(MASTER_SIGNAL, 0);
	rt_sleep(NTASKS*PERIOD);  // leave some time to clean them all up
	rt_sem_wait_barrier(&barrier);
	rt_printk("MASTER TASK ENDs\n");
	rt_task_suspend(0);
}

int init_module(void)
{
	int i;
	rt_sem_init(&barrier, NTASKS + 1);
	rt_set_oneshot_mode();
	start_rt_timer(0);
	rt_task_init(&mastertask, master_fun, 0, STKSZ, 0, 0, 0);
	rt_task_resume(&mastertask);
	for (i = 0; i < NTASKS; i++) {
		rt_task_init(&task[i], task_fun, i, STKSZ, 0, 1, 0);
		rt_task_resume(&task[i]);
	}
	printk("EXECUTION STARTED\n");
	return 0;
}

void cleanup_module(void)
{
	int i;
	stop_rt_timer();
	rt_sem_delete(&barrier);
	rt_task_delete(&mastertask);
	for (i = 0; i < NTASKS; i++) {
		rt_task_delete(&task[i]);
	}
	printk("EXECUTION ENDED\n");
}
