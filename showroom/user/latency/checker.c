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
#include <sys/mman.h>
#include <sys/poll.h>

#include <rtai_mbx.h>
#include <rtai_msg.h>

static RT_TASK *task;

int main(void)
{
	struct pollfd ufds = { 0, POLLIN, };
	unsigned int ch;
	MBX *mbx;
	long long max = -1000000000, min = 1000000000;
	struct sample { long long min; long long max; int index, ovrn; } samp;

	rt_allow_nonroot_hrt();
 	if (!(task = rt_task_init_schmod(nam2num("CHEKER"), 40, 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT MASTER TASK\n");
		exit(1);
	}

 	if (!(mbx = rt_get_adr(nam2num("LATMBX")))) {
		printf("CANNOT FIND MAILBOX\n");
		exit(1);
	}
	mlockall(MCL_CURRENT | MCL_FUTURE);

	while (1) {
		rt_mbx_receive(mbx, &samp, sizeof(samp));
		if (max < samp.max) max = samp.max;
		if (min > samp.min) min = samp.min;
		printf("* min: %lld/%lld, max: %lld/%lld average: %d (%d) <Hit [RETURN] to stop> *\n", samp.min, min, samp.max, max, samp.index, samp.ovrn);
		if (poll(&ufds, 1, 1)) {
			ch = getchar();
			break;
		}
	}

	rt_task_delete(task);
	exit(0);
}
