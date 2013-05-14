/*
COPYRIGHT (C) 2000  Paolo Mantegazza (mantegazza@aero.polimi.it)

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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <rtai_sem.h>
#include <rtai_msg.h>
#include "period.h"

pthread_t task[NTASKS];

int ntasks = NTASKS;

RT_TASK *mytask;

SEM *sem;

static int cpus_allowed;

void *thread_fun(void *arg)
{
	RTIME start_time, period;
	RTIME t0, t;
	SEM *sem;
	RT_TASK *mytask;
	unsigned long mytask_name;
	int mytask_indx, jit, maxj, maxjp, count;

	mytask_indx = *((int *)arg);
	mytask_name = taskname(mytask_indx);
	cpus_allowed = 1 - cpus_allowed;
 	if (!(mytask = rt_task_init_schmod(mytask_name, 1, 0, 0, SCHED_FIFO, 1 << cpus_allowed))) {
		printf("CANNOT INIT TASK %lu\n", mytask_name);
		exit(1);
	}
	printf("THREAD INIT: index = %d, name = %lu, address = %p.\n", mytask_indx, mytask_name, mytask);
	mlockall(MCL_CURRENT | MCL_FUTURE);

 	if (!(mytask_indx%2)) {
		rt_make_hard_real_time();
	}
	rt_receive(0, (unsigned long *)((void *)&sem));

	period = nano2count(PERIOD);
	start_time = rt_get_time() + nano2count(10000000);
	rt_task_make_periodic(mytask, start_time + (mytask_indx + 1)*period, ntasks*period);

// start of task body
	{
		count = maxj = 0;
		t0 = rt_get_cpu_time_ns();
		while(count++ < LOOPS) {
			rt_task_wait_period();
			t = rt_get_cpu_time_ns();
			if ((jit = t - t0 - ntasks*(RTIME)PERIOD) < 0) {
				jit = -jit;
			}
			if (count > 1 && jit > maxj) {
				maxj = jit;
			}
			t0 = t;
//			rtai_print_to_screen("THREAD: index = %d, count %d\n", mytask_indx, count);
		}
		maxjp = (maxj + 499)/1000;
	}
// end of task body

	rt_sem_signal(sem);
 	if (!(mytask_indx%2)) {
		rt_make_soft_real_time();
	}

	rt_task_delete(mytask);
	printf("THREAD %lu ENDS, LOOPS: %d MAX JIT: %d (us)\n", mytask_name, count, maxjp);
	return 0;
}

void endme(int dummy)
{
	rt_sem_delete(sem);
	stop_rt_timer();
	rt_task_delete(mytask);
	signal(SIGINT, SIG_DFL);
	exit(1);
}

int main(void)
{
	int i, indx[NTASKS];
	unsigned long mytask_name = nam2num("MASTER");

	signal(SIGINT, endme);

 	if (!(mytask = rt_task_init(mytask_name, 1, 0, 0))) {
		printf("CANNOT INIT TASK %lu\n", mytask_name);
		exit(1);
	}
	printf("MASTER INIT: name = %lu, address = %p.\n", mytask_name, mytask);

	sem = rt_sem_init(10000, 0);
	rt_set_oneshot_mode();
//	rt_set_periodic_mode();
	start_rt_timer(0);

	for (i = 0; i < ntasks; i++) {
		indx[i] = i;
		if (!(task[i] = rt_thread_create(thread_fun, &indx[i], 10000))) {
			printf("ERROR IN CREATING THREAD %d\n", indx[i]);
			exit(1);
 		}
 	}

	for (i = 0; i < ntasks; i++) {
		while (!rt_get_adr(taskname(i))) {
			rt_sleep(nano2count(20000000));
		}
	}

	for (i = 0; i < ntasks; i++) {
		rt_send(rt_get_adr(taskname(i)), (unsigned long)sem);
	}

	for (i = 0; i < ntasks; i++) {
		rt_sem_wait(sem);
	}

	for (i = 0; i < ntasks; i++) {
		while (rt_get_adr(taskname(i))) {
			rt_sleep(nano2count(20000000));
		}
	}

	for (i = 0; i < ntasks; i++) {
		rt_thread_join(task[i]);
	}
	rt_sem_delete(sem);
	stop_rt_timer();
	rt_task_delete(mytask);
	printf("MASTER %lu %p ENDS\n", mytask_name, mytask);
	return 0;
}
