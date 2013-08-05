/*
COPYRIGHT (C) 2000  Paolo Mantegazza <mantegazza@aero.polimi.it>

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
#include <sched.h>
#include <rtai_sem.h>
#include <rtai_msg.h>
#include "period.h"

#define ONE_SHOT

#define STACK_SIZE 1000

#define TASKBASE 1000

//RT_TASK thread[NTASKS];

int main(void)
{
	RTIME start_time, period;
	RTIME t0, t;
	RT_TASK *mytask;
	SEM *sem;
	unsigned long mytask_name;
	int i, mytask_indx, jit, maxj, maxjp, count;

	struct sched_param mysched;

	mytask_name = TASKBASE;
	mytask_indx = 0;
	sem = rt_sem_init(10000, 0);

	for (i = 1; i < NTASKS; i++) {
		if (!fork()) {
			mytask_name = TASKBASE + i;
			mytask_indx = i;
			break;
		}
	}

	mysched.sched_priority = sched_get_priority_max(SCHED_FIFO) - 1;
	if( sched_setscheduler( 0, SCHED_FIFO, &mysched ) == -1 ) {
		printf("ERROR IN SETTING THE POSIX SCHEDULER\n");
		exit(1);
 	}
	mlockall(MCL_CURRENT | MCL_FUTURE);

 	if (!(mytask = rt_task_init(mytask_name, 1, 0, 0))) {
		printf("CANNOT INIT TASK %lu\n", mytask_name);
		exit(1);
	}
	printf("TASK INIT: index = %d, name = %lu, address = %p.\n", mytask_indx, mytask_name, mytask);

	if (!mytask_indx) {
		rt_set_oneshot_mode();
		start_rt_timer(0);
		for (i = 1; i < NTASKS; i++) {
			while (!rt_get_adr(TASKBASE + i)) {
				rt_sleep(nano2count(1000000));
			}
		}
	}

 	if (!(mytask_indx%2)) {
		rt_make_hard_real_time();
	}

	if (!mytask_indx) {
		for (i = 1; i < NTASKS; i++) {
			rt_send((struct rt_task_struct *)rt_get_adr(TASKBASE + i), (unsigned long)sem);
		}
	} else {
		rt_receive(0, (unsigned int *)(unsigned long)&sem);
	}

	period = nano2count(PERIOD);
	start_time = rt_get_time() + nano2count(10000000);
	rt_task_make_periodic(mytask, start_time + (mytask_indx + 1)*period, NTASKS*period);

// start of task body
	{
		count = maxj = 0;
		t0 = rt_get_cpu_time_ns();
		while(count++ < LOOPS) {
			rt_task_wait_period();
			t = rt_get_cpu_time_ns();
			if ((jit = t - t0 - NTASKS*(RTIME)PERIOD) < 0) {
				jit = -jit;
			}
			if (count > 1 && jit > maxj) {
				maxj = jit;
			}
			t0 = t;
//			rtai_print_to_screen("TASK: index = %d, count %d\n", mytask_indx, count);
		}
		maxjp = (maxj + 499)/1000;
	}
// end of task body

	if (mytask_indx) {
		rt_sem_wait(sem);
	} else {
		for (i = 1; i < NTASKS; i++) {
			rt_sem_signal(sem);
		}
	}

 	if (!(mytask_indx%2)) {
		rt_make_soft_real_time();
	}
	if (!mytask_indx) {
		rt_sleep(nano2count(10000000));
		for (i = 1; i < NTASKS; i++) {
			while (rt_get_adr(TASKBASE + i)) {
				rt_sleep(nano2count(1000000));
			}
		}
		rt_sem_delete(sem);
		stop_rt_timer();
	}
	rt_task_delete(mytask);
	printf("TASK %lu ENDS, LOOPS: %d MAX JIT: %d (us)\n", mytask_name, count, maxjp);

	return 0;
}
