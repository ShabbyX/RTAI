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


#include <linux/kernel.h>
#include <linux/module.h>

#include <asm/io.h>

#include <rtai.h>
#include <rtai_fifos.h>
#include <rtai_sched.h>
#include <rtai_leds.h>

#include <rtai_math.h>

MODULE_LICENSE("GPL");

#define ONE_SHOT

#define TIMER_TO_CPU 3 // < 0 || > 1 to maintain a symmetric processed timer.
#define RUNNABLE_ON_CPUS 3  // 1: on cpu 0 only, 2: on cpu 1 only, 3: on any.
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define RUN_ON_CPUS (smp_num_cpus > 1 ? RUNNABLE_ON_CPUS : 1)
#else
#define RUN_ON_CPUS (num_online_cpus() > 1 ? RUNNABLE_ON_CPUS : 1)
#endif

#define TICK_PERIOD 100000 // nanoseconds
#define STACK_SIZE 2000
#define LOOPS 1000000000
#define PERIOD .00005

#define RTF 0
#define CMD 1

static RT_TASK mytask;

static int cpu_used[NR_RT_CPUS];

static void fun(long t) {
	int flag;
	double sine[2] = {1., 2.,}, time;
	unsigned int loops = LOOPS;

	time = 0.0;
	while (loops--) {
		cpu_used[hard_cpu_id()]++;
		time += (TICK_PERIOD/1.0E9);
		sine[0] = (6.28/PERIOD)*time;
		sine[1] = sin(sine[0]);
		t = sine[1] > 0. ? 1 : 0;
		rtf_put(RTF, sine, sizeof(sine));
		if (rtf_get(CMD, &flag, sizeof(flag)) == sizeof(flag)) {
			rt_task_use_fpu(rt_whoami(), flag);
		}
		rt_task_wait_period();
	}
}


int init_module(void)
{
	RTIME now, tick_period;
#ifdef ONE_SHOT
	rt_set_oneshot_mode();
#endif
	rtf_create(RTF, 4000);
	rtf_create(CMD, 100);
	rt_task_init(&mytask, fun, 0, STACK_SIZE, 0, 1, 0);
	rt_set_runnable_on_cpus(&mytask, RUN_ON_CPUS);
	tick_period = start_rt_timer((int)nano2count(TICK_PERIOD));
	rt_assign_irq_to_cpu(TIMER_8254_IRQ, TIMER_TO_CPU);
	rt_linux_use_fpu(1);
	now = rt_get_time();
	rt_task_make_periodic(&mytask, now + 2*tick_period, tick_period);
	return 0;
}


void cleanup_module(void)
{
	int cpuid;
	rt_reset_irq_to_sym_mode(TIMER_8254_IRQ);
	stop_rt_timer();
	rt_busy_sleep(10000000);
	rt_task_delete(&mytask);
	rtf_destroy(RTF);
	rtf_destroy(CMD);
	printk("\n\nCPU USE SUMMARY\n");
	for (cpuid = 0; cpuid < NR_RT_CPUS; cpuid++) {
		printk("# %d -> %d\n", cpuid, cpu_used[cpuid]);
	}
	printk("END OF CPU USE SUMMARY\n\n");
}
