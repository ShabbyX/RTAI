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
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <sys/poll.h>

#include <rtai_msg.h>

static int end;
static void endme(int dummy) { end = 1; }

int main(void)
{
	RT_TASK *task;
	unsigned long maxj;

	signal(SIGINT, endme);
	task = rt_task_init_schmod(nam2num("MNTASK"), 0, 0, 0, SCHED_OTHER, 0xF);
	rt_rpc(rt_get_adr(nam2num("RPCTSK")), (unsigned long)task, &maxj);

	while(!end) {
		rt_receive(NULL, &maxj);
		printf(">>> maxj = %lu (us).\n", (maxj + 499)/1000);
	}
	return 0;
}
