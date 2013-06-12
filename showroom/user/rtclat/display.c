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

#include <rtai_mbx.h>
#include <rtai_msg.h>

static RT_TASK *task;

int main(void)
{
	fd_set input;
	struct timeval tv;
	unsigned int msg, ch;
	MBX *mbx;
	long long max = -1000000000, min = 1000000000;
	struct sample { long long min; long long max; int index, ovrn; } samp;

	tv.tv_sec = 0;

 	if (!(task = rt_task_init(nam2num("LATCHK"), 20, 0, 0))) {
		printf("CANNOT INIT MASTER TASK\n");
		exit(1);
	}

 	if (!(mbx = rt_get_adr(nam2num("LATMBX")))) {
		printf("CANNOT FIND MAILBOX\n");
		exit(1);
	}
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	while (1) {
		rt_mbx_receive(mbx, &samp, sizeof(samp));
		if (max < samp.max) max = samp.max;
		if (min > samp.min) min = samp.min;
		printf("* min: %lld/%lld, max: %lld/%lld average: %d (%d) <Hit [RETURN] to stop> *\n", samp.min, min, samp.max, max, samp.index, samp.ovrn);
		FD_ZERO(&input);
		FD_SET(0, &input);
		tv.tv_usec = 20000;
		if (select(1, &input, NULL, NULL, &tv)) {
			ch = getchar();
			break;
		}
	}

	rt_rpc(rt_get_adr(nam2num("LATCAL")), msg, &msg);
	rt_task_delete(task);
	exit(0);
}
