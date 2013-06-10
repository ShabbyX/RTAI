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
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <rtai_lxrt.h>
#include <rtai_mbx.h>
#include <rtai_msg.h>

static RT_TASK *task;

int main(void)
{
	fd_set input;
	struct timeval tv;
	unsigned int msg, ch;
	MBX *mbx;
	struct sample { long long min; long long max; int index; double s; int ts; } samp;

	tv.tv_sec = 0;

 	if (!(task = rt_task_init(nam2num("LATCHK"), 20, 0, 0))) {
		printf("CANNOT INIT MASTER TASK\n");
		exit(1);
	}

 	if (!(mbx = rt_get_adr(nam2num("LATMBX")))) {
		printf("CANNOT FIND MAILBOX\n");
		exit(1);
	}

	while (1) {
		rt_mbx_receive(mbx, &samp, sizeof(samp));
		printf("*** min: %d, max: %d average: %d, dot %f ts %d (ent ends check, a/ent both) ***\n", (int) samp.min, (int) samp.max, samp.index, samp.s, samp.ts);
		FD_ZERO(&input);
		FD_SET(0, &input);
		tv.tv_usec = 20000;
	        if (select(1, &input, NULL, NULL, &tv)) {
			ch = getchar();
			break;
		}
	}

	if (ch == 'a') {
		rt_rpc(rt_get_adr(nam2num("LATCAL")), msg, &msg);
	}
	rt_task_delete(task);
	exit(0);
}
