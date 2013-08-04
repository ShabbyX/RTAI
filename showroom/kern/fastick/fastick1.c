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
#include <rtai_fifos.h>
#include <rtai_sched.h>

MODULE_LICENSE("GPL");

#define ONE_SHOT

#define TICK_PERIOD 100000

#define STACK_SIZE 2000

#define CMDF 0

static RT_TASK thread;

static int cpu_used[NR_RT_CPUS];

static void intr_handler(long t)
{
	char wakeup;
	while(1) {
		cpu_used[hard_cpu_id()]++;
		rtf_put(CMDF, &wakeup, sizeof(wakeup));
		rt_task_wait_period();
	}
}


int init_module(void)
{
	RTIME tick_period;
	rtf_create_using_bh(CMDF, 4, 0);
#ifdef ONE_SHOT
	rt_set_oneshot_mode();
#endif
	tick_period = start_rt_timer(nano2count(TICK_PERIOD));
	rt_task_init(&thread, intr_handler, 0, STACK_SIZE, 0, 0, 0);
	rt_task_make_periodic(&thread, rt_get_time() + 2*tick_period, tick_period);
	return 0;
}


void cleanup_module(void)
{
	int cpuid;
	stop_rt_timer();
	rt_busy_sleep(10000000);
	rt_task_delete(&thread);
	rtf_destroy(CMDF);
	printk("\n\nCPU USE SUMMARY\n");
	for (cpuid = 0; cpuid < NR_RT_CPUS; cpuid++) {
		printk("# %d -> %d\n", cpuid, cpu_used[cpuid]);
	}
	printk("END OF CPU USE SUMMARY\n\n");
}
