/*
COPYRIGHT (C) 2001  Paolo Mantegazza <mantegazza@aero.polimi.it>

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
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXLOOPS 100

#include <rtai_netrpc.h>

static volatile int end;

static void *endme(void *args)
{
	getchar();
	end = 1;
	return 0;
}

static pthread_t thread;

int main(int argc, char *argv[])
{
	unsigned long rcvnode;
	RT_TASK *sndtsk, *rcvtsk;
	MBX *mbx;
	long rcvport, i;
	struct sockaddr_in addr;

	thread = rt_thread_create(endme, NULL, 2000);

	if (!(sndtsk = rt_task_init_schmod(nam2num("SNDTSK"), 1, 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT SENDER TASK\n");
		exit(1);
	}
	rcvnode = 0;
	if (argc == 2 && strstr(argv[1], "RcvNode=")) {
		inet_aton(argv[1] + 8, &addr.sin_addr);
		rcvnode = addr.sin_addr.s_addr;
	}
	if (!rcvnode) {
		inet_aton("127.0.0.1", &addr.sin_addr);
		rcvnode = addr.sin_addr.s_addr;
	}
	mbx = rt_mbx_init(nam2num("SNDMBX"), 500);
	while ((rcvport = rt_request_port(rcvnode)) <= 0 && rcvport != -EINVAL);
	printf("\nSENDER TASK RUNNING\n");
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	for (i = 0; i < MAXLOOPS && !end; i++) {
		rt_mbx_send(mbx, &i, sizeof(long));
		rt_printk("SENT %ld\n", i);
		RT_sleep(rcvnode, rcvport, 200000000);
	}
	i = -1;
	rt_mbx_send(mbx, &i, sizeof(long));
	rt_make_soft_real_time();

	while (!(rcvtsk = RT_get_adr(rcvnode, rcvport, "RCVTSK"))) {
		RT_sleep(rcvnode, rcvport, 100000000);
	}
	RT_rpc(rcvnode, rcvport, rcvtsk, i, &i);
	rt_release_port(rcvnode, rcvport);
	rt_mbx_delete(mbx);
	rt_task_delete(sndtsk);
	printf("\nSENDER TASK STOPS\n");
	return 0;
}
