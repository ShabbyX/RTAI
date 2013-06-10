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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rtai_netrpc.h>
#include "period.h"

int main(int argc, char *argv[])
{
	MBX *mbx;
	SEM *sem;
	RT_TASK *task;
	RTIME t0, t;
	int i, count, jit, maxj, test_time, hdlnode, hdlport;
	struct sockaddr_in addr;
	unsigned long run;
	char c;

 	if (!(task = rt_task_init_schmod(nam2num("PRCTSK"), 1, 0, 0, SCHED_FIFO, TASK_CPU))) {
		printf("CANNOT INIT PROCESS TASK\n");
		exit(1);
	}

        hdlnode = 0;
        if (argc == 2 && strstr(argv[1], "HdlNode=")) {
                inet_aton(argv[1] + 8, &addr.sin_addr);
                hdlnode = addr.sin_addr.s_addr;
        }
        if (!hdlnode) {
                inet_aton("127.0.0.1", &addr.sin_addr);
                hdlnode = addr.sin_addr.s_addr;
        }
	while ((hdlport = rt_request_port(hdlnode)) <= 0 && hdlport != -EINVAL);
	mbx = RT_get_adr(hdlnode, hdlport, "HDLMBX");
        sem = rt_sem_init(nam2num("PRCSEM"), 0);

	printf("USE: SEM SEND/WAIT (s), TASK RESM/SUSP (r), INTERTASK MSG (m): [s|r|m]? ");
	scanf("%c", &c);
	switch (c) {
		case 'r': printf("TESTING TASK SUSPEND/RESUME TIME (s): ");
			  run = 2;
			  break;
		case 'm': printf("TESTING INTERTASK SEND/RECEIVE TIME (s): ");
			  run = 3;
			  break;
		default:  printf("TESTING SEMAPHORE WAIT/SEND TIME (s): ");
			  run = 1;
			  break;
	}
	scanf("%d", &test_time);
	printf("... WAIT FOR %d SECONDS (RUNNING AT %d hz).\n", test_time, 1000000000/PERIOD);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

        RT_mbx_send(hdlnode, hdlport, mbx, &task, sizeof(task));
        RT_mbx_send(hdlnode, hdlport, mbx, &sem, sizeof(sem));
        RT_mbx_send(hdlnode, hdlport, mbx, &run, sizeof(run));

	count = maxj = 0;
	t0 = rt_get_cpu_time_ns();
	while(++count <= test_time*(1000000000/(PERIOD*100))) {
		for (i = 0; i < 100; i++) {
			t = rt_get_cpu_time_ns();
			if ((jit = t - t0 - PERIOD) < 0) {
				jit = -jit;
			}
			if (count > 1 && jit > maxj) {
				maxj = jit;
			}
			t0 = t;
			switch (run) {
				case 1: rt_sem_wait(sem);
					break;
				case 2: rt_task_suspend(task);
					break;
				case 3: rt_receive(0, &run);
					break;
			}
		}
	}

	run = 0;
        RT_mbx_send(hdlnode, hdlport, mbx, &run, sizeof(run));
	rt_make_soft_real_time();
	rt_release_port(hdlnode, hdlport);
	rt_task_delete(task);
	printf("***** MAX JITTER %3d (us) *****\n", (maxj + 499)/1000);
	exit(0);
}
