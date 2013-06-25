/*
COPYRIGHT (C) 2001  Paolo Mantegazza (mantegazza@aero.polimi.it)

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
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rtai_netrpc.h>

static volatile int end;

static MBX *mbx;

static int SERVER;

static void *async_fun(void *args)
{
	RT_TASK *asynctsk;

 	if (!(asynctsk = rt_task_init_schmod(nam2num("ASYTSK"), 2, 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT ASYNC TASK\n");
		exit(1);
	}
	mlockall(MCL_CURRENT | MCL_FUTURE);
	while (!end) {
if (SERVER) {
		unsigned long long retval;
		long i1, i2;
		int l1, l2;
		l1 = l2 = sizeof(long);
		rt_get_net_rpc_ret(mbx, &retval, &i1, &l1, &i2, &l2, 0LL, MBX_RECEIVE);
		printf("SERVER ASYNC MSG: RETVAL = %llu, MSG = %ld, LEN = %d.\n", retval, i1, l1);
		if (i1 < 0) {
			end = 1;
			break;
		}
} else {
		rt_sleep(nano2count(100000000));
}
	}
	rt_task_delete(asynctsk);
	printf("\nASYNC SERVER TASK STOPS\n");
	return (void *)0;
}

static pthread_t athread;

int main(int argc, char *argv[])
{
	unsigned long sndnode;
	long sndport, i, r;
	RT_TASK *rcvtsk, *sndtsk;
	struct sockaddr_in addr;
	static MBX *sndmbx;

	SERVER = atoi(argv[1]);
 	if (!(rcvtsk = rt_task_init_schmod(nam2num("RCVTSK"), 2, 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT RECEIVER TASK\n");
		exit(1);
	}

	mbx = rt_mbx_init(nam2num("MBX"), 100);
	athread = rt_thread_create(async_fun, NULL, 10000);
	sndnode = 0;
	if (argc == 3 && strstr(argv[2], "SndNode=")) {
		inet_aton(argv[2] + 8, &addr.sin_addr);
		sndnode = addr.sin_addr.s_addr;
	}
	if (!sndnode) {
		inet_aton("127.0.0.1", &addr.sin_addr);
		sndnode = addr.sin_addr.s_addr;
	}

	rt_set_oneshot_mode();
	start_rt_timer(0);
	while ((sndport = rt_request_port_mbx(sndnode, mbx)) <= 0 && sndport != -EINVAL);
	while (!(sndmbx = RT_get_adr(sndnode, sndport, "SNDMBX"))) {
		rt_sleep(nano2count(100000000));
	}
	sndtsk = RT_get_adr(sndnode, sndport, "SNDTSK");
	printf("\nRECEIVER TASK RUNNING\n");
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	while (!end) {
		r = RT_mbx_receive(sndnode, -sndport, sndmbx, &i, sizeof(long));
		rt_printk("RECEIVE %ld %ld\n", r, i);
if (SERVER) {
		rt_sleep(nano2count(100000000));
} else {
		while (!end && rt_waiting_return(sndnode, sndport)) {
			rt_sleep(nano2count(1000000));
		}
		if (!end) {
			unsigned long long retval;
			long i1, i2;
			int l1, l2;
			if (rt_sync_net_rpc(sndnode, -sndport)) {
				l1 = l2 = sizeof(long);
				rt_get_net_rpc_ret(mbx, &retval, &i1, &l1, &i2, &l2, 0LL, MBX_RECEIVE);
				rt_printk("RECEIVER ASYNC MSG: RETVAL = %d, MSG = %ld, LEN = %d.\n", (int)retval, i1, l1);
				if (i1 < 0) {
					end = 1;
					break;
				}
			}
		}
}
	}

	rt_mbx_delete(mbx);
	rt_release_port(sndnode, sndport);
	rt_make_soft_real_time();
	rt_thread_join(athread);
	rt_return(rt_receive(0, &i), i);
	rt_task_delete(rcvtsk);
	stop_rt_timer();
	printf("\nRECEIVER TASK STOPS\n");
	return 0;
}
