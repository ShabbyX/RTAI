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
#include <sys/poll.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#include <rtai_sem.h>
#include <rtai_msg.h>

static int USE_RPC, SNDBRCV, HARDMAIN;

pthread_t thread;

RT_TASK *maintask, *funtask;

void *thread_fun(void *arg)
{
 	funtask = rt_task_init_schmod(0xcaccb, 0, 0, 0, SCHED_FIFO, 0x1);
	rt_printk("FUN INIT\n");
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	if (!SNDBRCV) {
		rt_sleep(nano2count(100000000));
	}

	if (USE_RPC) {
		unsigned long msg;
		rt_printk("FUN RPC\n");
	        rt_rpc(maintask, 0, &msg);
	} else {
		rt_printk("FUN SEND\n");
        	rt_send(maintask, 0);
		rt_printk("FUN SUSP\n");
        	rt_task_suspend(funtask);
	}
	rt_printk("FUN DONE\n");

	rt_task_delete(funtask);
	rt_printk("FUN END\n");
	return 0;
}

int main(int argc, char *argv[])
{
	unsigned long msg;
	int prio, bprio;

	if (argc > 1) {
		USE_RPC  = atoi(argv[1]);
		SNDBRCV  = atoi(argv[2]);
		HARDMAIN = atoi(argv[3]);
	}

 	if (!(maintask = rt_task_init_schmod(0xcacca, 1, 0, 0, SCHED_FIFO, 0x1))) {
		rt_printk("CANNOT INIT MAIN\n");
		exit(1);
	}
	start_rt_timer(0);
	rt_printk("MAIN INIT\n");

	pthread_create(&thread, NULL, thread_fun, NULL);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	if (HARDMAIN) {
		rt_make_hard_real_time();
	}

	rt_get_priorities(maintask, &prio, &bprio);
	rt_printk("TEST: %s, %s, %d\n", USE_RPC ? "WITH RPC" : "WITH SUSP/RESM", SNDBRCV ? "SEND BEFORE RECEIVE" : "RECEIVE BEFORE SEND", prio);

	if (SNDBRCV) {
		rt_sleep(nano2count(100000000));
	}

	rt_get_priorities(maintask, &prio, &bprio);
	rt_printk("MAIN REC %d\n", prio);
	if (USE_RPC) {
		RT_TASK *task;
		task = rt_receive(0, &msg);
		rt_get_priorities(maintask, &prio, &bprio);
		rt_printk("MAIN RET %d\n", prio);
		rt_return(task, 0);
	} else {
		rt_receive(0, &msg);
		rt_get_priorities(maintask, &prio, &bprio);
		rt_printk("MAIN RES %d\n", prio);
		rt_task_resume(funtask);
	}
	rt_get_priorities(maintask, &prio, &bprio);
	rt_printk("MAIN DONE %d\n", prio);

	stop_rt_timer();
	rt_task_delete(maintask);
	rt_printk("MAIN END\n");
	return 0;
}
