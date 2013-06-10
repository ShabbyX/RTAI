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

#include <sched.h>
#include <signal.h>

#include <rtai_lxrt.h>

#define PERIOD 500000

static int end;

static void endme(int dummy) { end = 1; }

int main(void)
{
	RTIME period;
	RT_TASK *mytask;
	unsigned long testb;

	signal(SIGINT, endme);
	testb = nam2num("HRTTSK");
 	if (!(mytask = rt_task_init(nam2num("MYTASK"), 2, 0, 0))) {
		printf("CANNOT INIT TESTA MASTER TASK\n");
		exit(1);
	}

	rt_set_oneshot_mode();
	period = start_rt_timer(nano2count(PERIOD));
	while(!rt_get_adr(testb));
	rt_task_make_periodic(rt_get_adr(testb), rt_get_time() + period, period);
	while (!end) {
		rt_sleep(nano2count(400000000));
		printf("SUPERVISOR WAITING FOR Ctrl-C\n");
	}
	rt_set_usp_flags(rt_get_adr(testb), FORCE_SOFT);
	while(rt_get_adr(testb)) {
		rt_sleep(nano2count(10000000));
	}
	stop_rt_timer();
	rt_task_delete(mytask);
	printf("SUPERVISOR ENDED, NOW WAIT ITS END.\n");
	return 0;
}
