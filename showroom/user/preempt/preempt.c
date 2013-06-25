/*
COPYRIGHT (C) 2003-2008 Paolo Mantegazza (mantegazza@aero.polimi.it)

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


#include <sys/mman.h>
#include <pthread.h>
#include <stdlib.h>
#include <rtai_fifos.h>
#include <rtai_sem.h>
#include <rtai_mbx.h>
#include <rtai_msg.h>

#define CPUMAP 0xF

#define USEDFRAC 20  // in %

#define SHOWPREEMPT  0
#if SHOWPREEMPT
#define BEGIN(x)  rt_printk(x)
#define END(x)    rt_printk(x)
#else
#define BEGIN(x)
#define END(x)
#endif

#define NAVRG 1000

#define FASTMUL  2

#define SLOWMUL  12

#ifdef CONFIG_UCLINUX
#define TICK_TIME 1000000
#else
#define TICK_TIME 100000
#endif

static RT_TASK *Latency_Task;
static RT_TASK *Slow_Task;
static RT_TASK *Fast_Task;

static volatile RTIME start;
static volatile int end, slowjit, fastjit;

static SEM *barrier;

static void *slow_fun(void *arg)
{
	int jit, period;
	RTIME expected;

	if (!(Slow_Task = rt_thread_init(nam2num("SLWTSK"), 3, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT SLOW TASK\n");
		exit(1);
	}

	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();
	rt_sem_wait_barrier(barrier);
	period = nano2count(SLOWMUL*TICK_TIME);
	expected = start + 9*nano2count(TICK_TIME);
	rt_task_make_periodic(Slow_Task, expected, period);
	while (!end) {
		jit = abs(count2nano(rt_get_time() - expected));
		if (jit > slowjit) {
			slowjit = jit;
		}
		rt_busy_sleep((SLOWMUL*TICK_TIME*USEDFRAC)/100);
		expected += period;
		END("SE\n");
		rt_task_wait_period();
		BEGIN("SB\n");
	}
	rt_sem_wait_barrier(barrier);
	rt_make_soft_real_time();
	rt_thread_delete(Slow_Task);
	return 0;
}

static void *fast_fun(void *arg)
{
	int jit, period;
	RTIME expected;

	if (!(Fast_Task = rt_thread_init(nam2num("FSTSK"), 2, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT FAST TASK\n");
		exit(1);
	}

	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();
	rt_sem_wait_barrier(barrier);
	period = nano2count(FASTMUL*TICK_TIME);
	expected = start + 6*nano2count(TICK_TIME);
	rt_task_make_periodic(Fast_Task, expected, period);
	while (!end) {
		jit = abs(count2nano(rt_get_time() - expected));
		if (jit > fastjit) {
			fastjit = jit;
		}
		rt_busy_sleep((FASTMUL*TICK_TIME*USEDFRAC)/100);
		expected += period;
		END("FE\n");
		rt_task_wait_period();
		BEGIN("FB\n");
	}
	rt_sem_wait_barrier(barrier);
	rt_make_soft_real_time();
	rt_thread_delete(Fast_Task);
	return 0;
}

static void *latency_fun(void *arg)
{
	struct sample { long min, max, avrg, jitters[2]; } samp;
	int diff;
	int skip;
	int average;
	int min_diff;
	int max_diff;
	int period;
	RT_TASK *chktsk;
	RTIME expected;

	min_diff = 1000000000;
	max_diff = -1000000000;
	if (!(Latency_Task = rt_thread_init(nam2num("PRETSK"), 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT LATENCY TASK\n");
		exit(1);
	}

	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();
	rt_sem_wait_barrier(barrier);
	period = nano2count(TICK_TIME);
	expected = start + 3*period;
	rt_task_make_periodic(Latency_Task, expected, period);
	while (!end) {
		average = 0;
		for (skip = 0; skip < NAVRG && !end; skip++) {
			expected += period;
			END("HE\n");
			rt_task_wait_period();
			BEGIN("HB\n");
			diff = count2nano(rt_get_time() - expected);
			if (diff < min_diff) {
				min_diff = diff;
			}
			if (diff > max_diff) {
				max_diff = diff;
			}
			average += diff;
		}
		samp.min = min_diff;
		samp.max = max_diff;
		samp.avrg = average/NAVRG;
		samp.jitters[0] = fastjit;
		samp.jitters[1] = slowjit;
		if ((chktsk = rt_get_adr(nam2num("PRECHK")))) {
			rt_sendx_if(chktsk, &samp, sizeof(samp));
		}
	}
	rt_sem_wait_barrier(barrier);
	rt_make_soft_real_time();
	rt_thread_delete(Latency_Task);
	return 0;
}

static pthread_t latency_thread, fast_thread, slow_thread;
static int hard_timer_running;

int main(void)
{
	RT_TASK *Main_Task;
	long msg;

	if (!(Main_Task = rt_thread_init(nam2num("MNTSK"), 10, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT MAIN TASK\n");
		exit(1);
	}

	if (!(hard_timer_running = rt_is_hard_timer_running())) {
		start_rt_timer(0);
	}
	barrier = rt_sem_init(nam2num("PREMS"), 4);
	latency_thread = rt_thread_create(latency_fun, NULL, 0);
	fast_thread    = rt_thread_create(fast_fun, NULL, 0);
	slow_thread    = rt_thread_create(slow_fun, NULL, 0);
	start = rt_get_time() + nano2count(200000000);
	rt_sem_wait_barrier(barrier);
	rt_receive(0, &msg);
	end = 1;
	rt_sem_wait_barrier(barrier);
	rt_thread_join(latency_thread);
	rt_thread_join(fast_thread);
	rt_thread_join(slow_thread);
	if (!hard_timer_running) {
		stop_rt_timer();
	}
	rt_sem_delete(barrier);
	rt_thread_delete(Main_Task);
	return 0;
}
