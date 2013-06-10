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
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>

#include <rtai_tasklets.h>

#define LOOPS   100
#define PERIOD  10000000

static volatile int end, data = LOOPS;

static struct rt_tasklet_struct *tasklet;

static int tskloops;

static void tasklet_handler(unsigned long data)
{
	if (tskloops++ < LOOPS) {
		rt_printk("\nTASKLET: %d, %d", tskloops, (*((int *)data))--);
	} else {
		end = 1;
	}
}

int main(void)
{
	start_rt_timer(0);
	rt_task_init_schmod(rt_get_name(0), 10, 0, 0, SCHED_OTHER, 0xF);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	tasklet = rt_init_tasklet();
	rt_insert_tasklet(tasklet, 0, tasklet_handler, (unsigned long)&data, nam2num("TSKLET"), 1);
	while(!end) {
		rt_exec_tasklet(tasklet);
		rt_sleep(nano2count(PERIOD));
	}
	rt_remove_tasklet(tasklet);
	rt_delete_tasklet(tasklet);
	rt_task_delete(NULL);
	return 0;
}
