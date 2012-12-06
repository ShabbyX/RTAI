/*
COPYRIGHT (C) 1999-2008  Paolo Mantegazza (mantegazza@aero.polimi.it)

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
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdlib.h>

#include <rtai_mbx.h>

#include "period.h"

#define OPT_MINUS(x)  (('-' << 8) + x)
#define OPT_EQ(x)     ((x << 8) + '=')

static int verbose, bug, semaforo, test_time;

int main(int argc, char *argv[])
{
	MBX *mbx;
	SEM *sem;
	RT_TASK *mytask;
	RTIME t0, t;
	int i, count, jit, maxj;
	char run, *opt;

	for(i = 1; i < argc; i++) {
		opt = argv[i];
		switch((*opt << 8) + *(opt + 1)) {
			case OPT_MINUS('s'): semaforo++; break;
			case OPT_MINUS('b'): bug++; break;
			case OPT_MINUS('v'): verbose++; break;
			break;
		}
	}

 	if (!(mytask = rt_thread_init(nam2num("PRCTSK"), 1, 0, SCHED_FIFO, TASK_CPU))) {
		printf("CANNOT INIT PROCESS TASK\n");
		exit(1);
	}
	if (!(mbx = rt_get_adr(nam2num("RESMBX")))) {
		printf("CANNOT FIND MAILBOX\n");
		exit(1);
	}
	if (!(sem = rt_get_adr(nam2num("RESEM")))) {
		printf("CANNOT FIND SEMAPHORE\n");
		exit(1);
	}
	printf("%s", semaforo ? "SEMAPHORE WAIT/SEND" : "TASK SUSPEND/RESUME");

	printf("\nTest time (s): ");
	scanf("%d", &test_time);
        printf("\n... DO NOT PANIC, WAIT FOR %d SECONDS (RUNNING AT %d hz).\n", test_time, 1000000000/PERIOD);

	mlockall(MCL_CURRENT | MCL_FUTURE);

	rt_make_hard_real_time();
	run = semaforo ? 1 : 2;

	t0 = 0;
	count = maxj = 0;
	rt_mbx_send(mbx, &run, 1);
	while(++count <= test_time*1000000000LL/PERIOD) {
		if (semaforo) {
			rt_sem_wait(sem);
		} else {
			rt_task_suspend(mytask);
		} 
		if (!t0) {
			t0 = rt_get_cpu_time_ns();
		} else {
			t = rt_get_cpu_time_ns();
			if ((jit = t - t0 - PERIOD) < 0) {
				jit = -jit;
			}
			if (jit > maxj) {
				maxj = jit;
				rt_printk("AT COUNT = %d, MAXJ = %d\n", count, maxj);
			}
			t0 = t;
		} 
//		rt_printk("> COUNT = %d\n", count);
	}

	run = 0;
	rt_mbx_send(mbx, &run, 1);
	rt_make_soft_real_time();
	if (!bug) rt_task_delete(mytask);
	printf("***** MAX JITTER %3d (us) *****\n", (maxj + 499)/1000);
	exit(0);
}
