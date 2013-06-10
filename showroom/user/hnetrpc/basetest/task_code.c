/*
COPYRIGHT (C) 2001  Paolo Mantegazza (mantegazza@aero.polimi.it),

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
#include <string.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rtai_netrpc.h>

static unsigned long comnode;

#define NUM_TASKS 4

static char *strs[NUM_TASKS] = { "Joey    ", "Johnny  ", "Dee Dee ", "Marky   "
};

static char *task[NUM_TASKS] = { "T1", "T2", "T3", "T4" };

static char *sem[NUM_TASKS] = { "S1", "S2", "S3", "S4" };
static SEM *sems[NUM_TASKS], *sync_sem, *prio_sem, *print_sem, *end_sem;

static MBX *mbx_in, *mbx_out;

static inline void RT_SEM_SIGNAL(unsigned long node, int port, SEM *sem)
{
	if (rt_waiting_return(node, port)) {
		RT_sem_signal(node, port > 0 ? port : -port, sem);
	} else {
		RT_sem_signal(node, port, sem);
	}
}

#define TAKE_PRINT  RT_sem_wait(comnode, srvport, print_sem);
#define GIVE_PRINT  RT_SEM_SIGNAL(comnode, -srvport, print_sem);

static void *task_code(void *arg)
{
	RT_TASK *mytask;
	int task_no, i, ret, srvport;
	char buf[9];

	task_no = *((int *)arg);
	buf[8] = 0;
        if (!(mytask = rt_thread_init(nam2num(task[task_no]), NUM_TASKS - task_no, 0, SCHED_FIFO, 0xF))) {
                printf("CANNOT INIT TASK TASK %d\n", task_no);
                exit(1);
        }
	srvport = rt_request_port(comnode);
printf("TASK_NO %d GOT ITS EXEC COMNODE PORT %lx, %d\n", task_no, comnode, srvport);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	for (i = 0; i < 5; ++i)	{
		RT_sem_wait(comnode, srvport, sems[task_no]);
		TAKE_PRINT;
		rt_printk(strs[task_no]);
		GIVE_PRINT;
		if (task_no == NUM_TASKS - 1) {
			TAKE_PRINT;
			rt_printk("\n");
			GIVE_PRINT;
		}
		RT_SEM_SIGNAL(comnode, -srvport, sems[(task_no + 1)%NUM_TASKS]);
	}
	RT_SEM_SIGNAL(comnode, -srvport, sync_sem);
	RT_sem_wait(comnode, srvport, prio_sem);
	TAKE_PRINT;
	rt_printk(strs[task_no]);
	GIVE_PRINT;
	RT_sleep(comnode, srvport, 1000000000LL);
	RT_sem_wait_timed(comnode, srvport, prio_sem, (task_no + 1)*1000000000LL);
	TAKE_PRINT;
	rt_printk("sem timeout, task %d, %s\n", task_no, strs[task_no]);
	GIVE_PRINT;
	RT_SEM_SIGNAL(comnode, -srvport, sync_sem);

/* message queue stuff */
	if ((ret = RT_mbx_receive(comnode, srvport, mbx_in, buf, 8)) != 0) {
		TAKE_PRINT;
		rt_printk("RT_mbx_receive() failed with %d\n", ret);
		GIVE_PRINT;
	}
	TAKE_PRINT;
	rt_printk("\nreceived by task %d ", task_no);
	rt_printk(buf);
	GIVE_PRINT;
	RT_mbx_send(comnode, srvport, mbx_out, strs[task_no], 8);
/* test receive timeout */
	RT_sem_wait(comnode, srvport, sync_sem);
	if (RT_mbx_receive_timed(comnode, srvport, mbx_in, buf, 8, (task_no + 1)*1000000000LL)) {
		TAKE_PRINT;
		rt_printk("mbx timeout, task %d, %s\n", task_no, strs[task_no]);
		GIVE_PRINT;
	}
	TAKE_PRINT;
	rt_printk("\ntask %d complete\n", task_no);
	GIVE_PRINT;
	rt_make_soft_real_time();
printf("TASK_NO %d REL ITS EXEC COMNODE PORT %lx, %d\n", task_no, comnode, srvport);
	RT_sem_signal(comnode, srvport, end_sem);
	rt_release_port(comnode, srvport);
	rt_task_delete(mytask);
	return (void *)0;
}

static int thread[NUM_TASKS];

void msleep(int ms)
{
        struct timeval timout;
        timout.tv_sec = 0;
        timout.tv_usec = ms*1000;
        select(1, NULL, NULL, NULL, &timout);
}

static int init_module(void)
{
	int i, srvport;

	while ((srvport = rt_request_port(comnode)) <= 0) {
		msleep(100);
	}
printf("TASK CODE GOT INIT COMNODE PORT %lx, %d\n", comnode, srvport);
	while (!(print_sem = RT_get_adr(comnode, srvport, "PRTSEM"))) {
		msleep(100);
	}
	sync_sem = RT_get_adr(comnode, srvport, "SYNCSM");
	prio_sem = RT_get_adr(comnode, srvport, "PRIOSM");
	end_sem = RT_get_adr(comnode, srvport, "ENDSEM");
	mbx_in   = RT_get_adr(comnode, srvport, "MBXIN");
	mbx_out  = RT_get_adr(comnode, srvport, "MBXOUT");
        for (i = 0; i < NUM_TASKS; i++) {
		sems[i] = RT_get_adr(comnode, srvport, sem[i]);
		if (!(thread[i] = rt_thread_create(task_code, (void *)&i, 10000))) {
                        printf("ERROR IN CREATING THREAD %d\n", i);
                        exit(1);
                }
		msleep(100);
        }
printf("TASK CODE REL INIT COMNODE PORT %lx, %d\n", comnode, srvport);
	rt_release_port(comnode, srvport);
	return 0;
}

int main(int argc, char *argv[])
{
        RT_TASK *task;
        struct sockaddr_in addr;
	int i, srvport;

        if (!(task = rt_task_init(nam2num("TSKCOD"), 0, 0, 0))) {
                printf("CANNOT INIT TASK CODE\n");
                exit(1);
        }
        comnode = 0;
        if (argc == 2 && strstr(argv[1], "ComNode=")) {
              	inet_aton(argv[1] + 8, &addr.sin_addr);
                comnode = addr.sin_addr.s_addr;
        }
        if (!comnode) {
                inet_aton("127.0.0.1", &addr.sin_addr);
                comnode = addr.sin_addr.s_addr;
        }
        init_module();
        for (i = 0; i < NUM_TASKS; i++) {
		rt_thread_join(thread[i]);
        }
	while ((srvport = rt_request_port(comnode)) <= 0) {
		msleep(100);
	}
	RT_sem_signal(comnode, srvport, end_sem);
	rt_release_port(comnode, srvport);
	rt_task_delete(task);
	exit(0);
}
