/*
COPYRIGHT (C) 1999-2008 Paolo Mantegazza (mantegazza@aero.polimi.it)

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
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <signal.h>
#include <unistd.h>

#include <rtai_mbx.h>
#include <rtai_msg.h>

static int end;

static void endme (int dummy) { end = 1; }

int main(int argc,char *argv[])
{
	RT_TASK *task;
	long msg;
	struct sample { long min, max, avrg, jitters[2]; } samp;

        if (!(task = rt_task_init_schmod(nam2num("PRECHK"), 15, 0, 0, SCHED_FIFO, 0xF))) {
                printf("CANNOT INIT MASTER TASK\n");
                exit(1);
        }

	signal (SIGINT, endme);

	while (!end) {
                struct pollfd pfd = { fd: 0, events: POLLIN|POLLERR|POLLHUP, revents: 0 };
		rt_receivex(0, &samp, sizeof(samp), (void *)&msg);
		printf("* latency: min: %ld, max: %ld, average: %ld; fastjit: %ld, slowjit: %ld * (all us) *\n", samp.min/1000, samp.max/1000, samp.avrg/1000, samp.jitters[0]/1000, samp.jitters[1]/1000);
		fflush(stdout);
                if (poll(&pfd, 1, 20) > 0 && (pfd.revents & (POLLIN|POLLERR|POLLHUP)) != 0) break;
        }
	rt_rpc(rt_get_adr(nam2num("MNTSK")), msg, &msg);
	return 0;
}
