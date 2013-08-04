/*
COPYRIGHT (C) 2008  Paolo Mantegazza <mantegazza@aero.polimi.it>

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

#include "params.h"
#include <rtai_lxrt.h>
#include <rtai_usi.h>

volatile int end;
static void endme(int unused) { end = 1; }

int main(void)
{
	RT_TASK *mytask;
	RTIME t0 = 0, t;
	int count, jit, maxj, echo = 0;

	signal(SIGHUP, endme);
	signal(SIGINT, endme);
	signal(SIGKILL, endme);
	signal(SIGTERM, endme);
	signal(SIGALRM, endme);

 	if (!(mytask = rt_task_init_schmod(nam2num("PRCTSK"), 1, 0, 0, SCHED_FIFO, TASK_CPU))) {
		printf("CANNOT INIT PROCESS TASK\n");
		exit(1);
	}

	count = RTC_FREQ;
	maxj = 0;

	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	while(!rt_task_suspend(mytask) && !end) {
		if (!count) {
			t = rt_get_cpu_time_ns();
			if ((jit = t - t0 - PERIOD) < 0) {
				jit = -jit;
			}
			if (jit > maxj) {
				maxj = jit;
			}
			t0 = t;
			if (echo++ > RTC_FREQ/5) {
				echo = 0;
				rt_printk("LATENCY %d, max = %d\n", jit, maxj);
			}
		} else {
			t0 = rt_get_cpu_time_ns();
			count--;
		}
	}

	rt_disable_irq(RTC_IRQ);
	rt_make_soft_real_time();
	rt_task_delete(mytask);
	return 0;
}
