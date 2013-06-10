/*
COPYRIGHT (C) 1999  Paolo Mantegazza (mantegazza@aero.polimi.it)

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


/* Produce a rectangular wave on output 0 of a parallel port */

#include <linux/module.h>

#include <asm/io.h>

#include <asm/rtai.h>
#include <rtai_sched.h>

#define ONE_SHOT

#define TICK_PERIOD 10000000

#define STACK_SIZE 2000

#define LOOPS 1000000000

#define NTASKS 8

static RT_TASK thread[NTASKS];

static RTIME tick_period;

static int cpu_used[NR_RT_CPUS];

static void fun(long t)
{
	unsigned int loops = LOOPS;
	while(loops--) {
		cpu_used[hard_cpu_id()]++;
		rt_printk("TASK %d %d\n", t, thread[t].priority);
		if (t == (NTASKS - 1)) {
			rt_printk("\n\n");
			rt_task_wait_period();
		} else {
			rt_task_set_resume_end_times(-NTASKS*tick_period, -(t + 1)*tick_period);
		}
	}
}


int init_module(void)
{
	RTIME now;
	int i;

#ifdef ONE_SHOT
	rt_set_oneshot_mode();
#endif
	for (i = 0; i < NTASKS; i++) {
		rt_task_init(&thread[i], fun, i, STACK_SIZE, NTASKS - i - 1, 0, 0);
	}
	tick_period = start_rt_timer(nano2count(TICK_PERIOD));
	now = rt_get_time() + NTASKS*tick_period;
	for (i = 0; i < NTASKS; i++) {
		rt_task_make_periodic(&thread[NTASKS - i - 1], now, NTASKS*tick_period);
	}
	return 0;
}


void cleanup_module(void)
{
	int i, cpuid;

	stop_rt_timer();
	for (i = 0; i < NTASKS; i++) {
		rt_task_delete(&thread[i]);
	}
	printk("\n\nCPU USE SUMMARY\n");
	for (cpuid = 0; cpuid < NR_RT_CPUS; cpuid++) {
		printk("# %d -> %d\n", cpuid, cpu_used[cpuid]);
	}
	printk("END OF CPU USE SUMMARY\n\n");
}
