/*
COPYRIGHT (C) 2003  Paolo Mantegazza (mantegazza@aero.polimi.it)

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
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>

#include <rtai_lxrt.h>

int main(int argc, char *argv[])
{
	int fifo, period, skip, average = 0;
	RT_TASK *task;
	RTIME expected;

        if ((fifo = open("/dev/rtf0", O_WRONLY)) < 0) {
                printf("Error opening FIFO0 in UCAL\n");
                exit(1);
        }
 	if (!(task = rt_task_init_schmod(nam2num("UCAL"), 0, 0, 0, SCHED_FIFO, 0xF))) {
		printf("Cannot init UCAL\n");
		exit(1);
	}

	rt_set_oneshot_mode();
	period = start_rt_timer(nano2count(atoi(argv[1])));
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();
	expected = rt_get_time() + 100*period;
	rt_task_make_periodic(task, expected, period);
	for (skip = 0; skip < atoi(argv[2]); skip++) {
		expected += period;
		rt_task_wait_period();
		average += (int)count2nano(rt_get_time() - expected);
	}
	rt_make_soft_real_time();
	stop_rt_timer();	
	rt_task_delete(task);
	write(fifo, &average, sizeof(average));
	close(fifo);
	exit(0);
}
