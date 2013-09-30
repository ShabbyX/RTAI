/*
COPYRIGHT (C) 2007  Paolo Mantegazza <mantegazza@aero.polimi.it>

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

/*
 * illustrating the use of a task switch signal from user space
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <sys/mman.h>

#include <rtai_signal.h>

static volatile int end;

#define PERIOD 100000
#define SWITCH_SIGNAL 1  // must be != 0

void catch_signal(int sig)
{
	end = 1;
}

static void switch_handler(long signal, RT_TASK *task)
{
	static unsigned long cnt = 0, rpt = 1000000000/PERIOD;
	if (++cnt > rpt) {
		rt_printk("# sw: %lu, tsk: %p, sig: %lu.\n", rpt, task, signal);
		rpt += 1000000000/PERIOD;
	}
}

int main(void)
{
	RT_TASK *task;

	signal(SIGTERM, catch_signal);
	signal(SIGINT,  catch_signal);

 	if (!(task = rt_thread_init(nam2num("SWITCH"), 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT SWITCH TASK SIGNAL\n");
		exit(1);
	}

	start_rt_timer(0);
	rt_request_signal(SWITCH_SIGNAL, switch_handler);
	rt_task_signal_handler(task, (void *)SWITCH_SIGNAL);
	rt_make_hard_real_time();

	while (!end) {
		rt_sleep(nano2count(PERIOD));
	}

	rt_task_signal_handler(task, NULL);
	rt_release_signal(SWITCH_SIGNAL, task);

	rt_make_soft_real_time();
	stop_rt_timer();
	rt_task_delete(task);

	return 0;
}
