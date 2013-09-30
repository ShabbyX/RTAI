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
#include <signal.h>

#include <rtai_lxrt.h>

static RT_TASK *hrttsk;

int main(void)
{
	unsigned long hrttsk_name = nam2num("HRTTSK");
	struct sched_param mysched;

	mysched.sched_priority = sched_get_priority_max(SCHED_FIFO);
	if( sched_setscheduler( 0, SCHED_FIFO, &mysched ) == -1 ) {
		puts("ERROR IN SETTING THE SCHEDULER");
		perror("errno");
		exit(0);
 	}
 	if (!(hrttsk = rt_task_init(hrttsk_name, 1, 0, 0))) {
		printf("CANNOT INIT TESTB MASTER TASK\n");
		exit(1);
	}

	rt_set_usp_flags_mask(FORCE_SOFT);
	rt_task_suspend(hrttsk);
	printf("BACKGROUND REAL TIME TASK IS HARD .....\n");
	mlockall(MCL_CURRENT | MCL_FUTURE);

	rt_make_hard_real_time();
	while(rt_is_hard_real_time(hrttsk)) {
		rt_task_wait_period();
	}
	printf("..... BACKGROUND REAL TIME TASK IS SOFT NOW, YOU CAN KILL IT BY HAND\n");
	rt_task_delete(hrttsk);
	while(1) { sleep(3); printf("BACKGROUND PROCESS STILL RUNNING\n"); }
}
