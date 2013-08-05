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


#include <linux/module.h>

#include <asm/io.h>

#include <asm/rtai.h>
#include <rtai_fifos.h>
#include <rtai_sched.h>
#include <rtai_sem.h>

#define IRQEXT

#define SEM_TYPE BIN_SEM

#define TICK 1000000 // nanoseconds

#define STACK_SIZE 2000

#define CMDF 0

SEM sem;

RT_TASK thread;

static int cpu_used[NR_RT_CPUS + 1];

static RTIME t0, t;
static int passed, maxj, jit;

static int Mode = 1;
RTAI_MODULE_PARM(Mode, int);

#ifndef IRQEXT

static void rt_timer_tick(void)
{
	cpu_used[NR_RT_CPUS]++;
	if (passed++ < 5) {
		t0 = rdtsc();
	} else {
		t = rdtsc();
		if ((jit = t - t0 - imuldiv(rt_times.periodic_tick, CPU_FREQ, FREQ_8254)) < 0) {
			jit = -jit;
		}
		if (jit > maxj) {
			maxj = jit;
		}
		t0 = t;
	}

	rt_times.tick_time = rt_times.intr_time;
	rt_times.intr_time = rt_times.tick_time + rt_times.periodic_tick;
	rt_set_timer_delay(0);
	if (rt_times.tick_time >= rt_times.linux_time) {
		if (rt_times.linux_tick > 0) {
			rt_times.linux_time += rt_times.linux_tick;
		}
		rt_pend_linux_irq(TIMER_8254_IRQ);
	}
	if (Mode) {
		rt_sem_signal(&sem);
	} else {
		rt_task_resume(&thread);
	}
}

#else

static int rt_timer_tick_ext(int irq, unsigned long data)
{
	int ret = 1;
	cpu_used[NR_RT_CPUS]++;
	if (passed++ < 5) {
		t0 = rdtsc();
	} else {
		t = rdtsc();
		if ((jit = t - t0 - imuldiv(rt_times.periodic_tick, CPU_FREQ, FREQ_8254)) < 0) {
			jit = -jit;
		}
		if (jit > maxj) {
			maxj = jit;
		}
		t0 = t;
	}

	rt_times.tick_time = rt_times.intr_time;
	rt_times.intr_time = rt_times.tick_time + rt_times.periodic_tick;
	rt_set_timer_delay(0);
	if (rt_times.tick_time >= rt_times.linux_time) {
		if (rt_times.linux_tick > 0) {
			rt_times.linux_time += rt_times.linux_tick;
		}
		rt_pend_linux_irq(TIMER_8254_IRQ);
		ret = 0;
	}
rt_sched_lock();
	if (Mode) {
		rt_sem_signal(&sem);
	} else {
		rt_task_resume(&thread);
	}
rt_sched_unlock();
	return ret;
}

#endif

static void intr_handler(long t) {

	while(1) {
		cpu_used[hard_cpu_id()]++;
		t = imuldiv(maxj, 1000000, CPU_FREQ);
		rtf_put(CMDF, &t, sizeof(int));
		if (Mode) {
			if (rt_sem_wait(&sem) >= SEM_TIMOUT) {
				return;
			}
		} else {
			rt_task_suspend(&thread);
		}
	}
}


#ifdef EMULATE_TSC
DECLR_8254_TSC_EMULATION;
#endif

int init_module(void)
{
	rt_assign_irq_to_cpu(0, 0);
	rtf_create(CMDF, 10000);
	if (Mode) {
		rt_typed_sem_init(&sem, 0, SEM_TYPE);
	}
	rt_task_init_cpuid(&thread, intr_handler, 0, STACK_SIZE, 0, 0, 0, 0);
	rt_task_resume(&thread);
#ifdef IRQEXT
	rt_request_timer((void *)rt_timer_tick_ext, imuldiv(TICK, FREQ_8254, 1000000000), 0);
	rt_set_global_irq_ext(0, 1, 0);
#else
	rt_request_timer((void *)rt_timer_tick, imuldiv(TICK, FREQ_8254, 1000000000), 0);
#endif
#ifdef EMULATE_TSC
	SETUP_8254_TSC_EMULATION;
#endif
	return 0;
}


void cleanup_module(void)
{
	int cpuid;
	rt_reset_irq_to_sym_mode(0);
#ifdef EMULATE_TSC
	CLEAR_8254_TSC_EMULATION;
#endif
	rt_free_timer();
	rtf_destroy(CMDF);
	rt_task_delete(&thread);
	if (Mode) {
		rt_sem_delete(&sem);
	}
	printk("\n\nCPU USE SUMMARY\n");
	for (cpuid = 0; cpuid < NR_RT_CPUS; cpuid++) {
		printk("# %d -> %d\n", cpuid, cpu_used[cpuid]);
	}
	printk("# ints -> %d\n",  cpu_used[NR_RT_CPUS]);
	printk("END OF CPU USE SUMMARY\n\n");
}
