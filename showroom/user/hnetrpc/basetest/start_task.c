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

static unsigned long comnode, tasknode;

#define NUM_TASKS 4

static char *task[NUM_TASKS] = { "T1", "T2", "T3", "T4" };
static char *strs[NUM_TASKS] = { "Joey    ", "Johnny  ", "Dee Dee ", "Marky   " };
static char sync_str[] = "sync\n";

static char *sem[NUM_TASKS] = { "S1", "S2", "S3", "S4" };
static SEM *sems[NUM_TASKS], *sync_sem, *prio_sem, *print_sem, *end_sem;

//static RT_TASK *start_task;

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

static void *start_task_code(void *args)
{
	int i, srvport;
	char buf[9];
	RT_TASK *task;

	buf[8] = 0;
	if (!(task = rt_thread_init(getpid(), 10, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT START_TASK BUDDY\n");
		exit(1);
	}

	srvport = rt_request_hard_port(comnode);
printf("START TASK GOT ITS EXEC COMNODE PORT %lx, %d\n", comnode, srvport);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	RT_SEM_SIGNAL(comnode, -srvport, sems[0]);
	for (i = 0; i < NUM_TASKS; ++i) {
		RT_sem_wait(comnode, srvport, sync_sem);
	}
	TAKE_PRINT;
	rt_printk(sync_str);
	GIVE_PRINT;
	for (i = 0; i < NUM_TASKS; ++i) {
		RT_SEM_SIGNAL(comnode, -srvport, prio_sem);
	}
	TAKE_PRINT;
	rt_printk("\n");
	GIVE_PRINT;
	for (i = 0; i < NUM_TASKS; ++i) {
		RT_sem_wait(comnode, srvport, sync_sem);
	}
	TAKE_PRINT;
	rt_printk(sync_str);
	GIVE_PRINT;

	TAKE_PRINT;
	rt_printk("testing message queues\n");
	GIVE_PRINT;
	for (i = 0; i < NUM_TASKS; ++i) {
		if (RT_mbx_send(comnode, srvport, mbx_in, strs[i], 8)) {
			TAKE_PRINT;
			rt_printk("RT_mbx_send() failed\n");
			GIVE_PRINT;
		}
	}
	for (i = 0; i < NUM_TASKS; ++i) {
		RT_mbx_receive(comnode, srvport, mbx_out, buf, 8);
		TAKE_PRINT;
		rt_printk("\nreceived from mbx_out: %s", buf);
		GIVE_PRINT;
	}
	TAKE_PRINT;
	rt_printk("\n");
	GIVE_PRINT;
	for (i = 0; i < NUM_TASKS; ++i) {
		RT_SEM_SIGNAL(comnode, -srvport, sync_sem);
	}
	TAKE_PRINT;
	rt_printk("\ninit task complete\n");
	GIVE_PRINT;
printf("START TASK REL ITS EXEC COMNODE PORT %lx, %d\n", comnode, srvport);
	RT_sem_signal(comnode, srvport, end_sem);
	rt_make_soft_real_time();
	rt_release_port(comnode, srvport);
	rt_task_delete(task);
	return (void *)0;
}

void msleep(int ms)
{
#include <sys/poll.h>
	struct timeval timout;
	timout.tv_sec = 0;
	timout.tv_usec = ms*1000;
//      select(1, NULL, NULL, NULL, &timout);
	poll(0, 0, ms);
}

static int thread;

static int init_module(void)
{
	int i, srvport;
	while ((srvport = rt_request_hard_port(tasknode)) <= 0) {
		msleep(100);
	}
printf("START TASK GOT SYNC TASKNODE PORT %lx, %d\n", tasknode, srvport);
	rt_make_hard_real_time();
	while (!RT_get_adr(tasknode, srvport, task[NUM_TASKS - 1])) {
		msleep(100);
	}
	rt_make_soft_real_time();
printf("START TASK REL SYNC TASKNODE PORT %lx, %d\n", tasknode, srvport);
	rt_release_port(tasknode, srvport);
	srvport = rt_request_hard_port(comnode);
printf("START TASK GOT INIT COMNODE PORT %lx, %d\n", comnode, srvport);
	rt_make_hard_real_time();
	print_sem = RT_get_adr(comnode, srvport, "PRTSEM");
	sync_sem = RT_get_adr(comnode, srvport, "SYNCSM");
	prio_sem = RT_get_adr(comnode, srvport, "PRIOSM");
	end_sem = RT_get_adr(comnode, srvport, "ENDSEM");
	mbx_in   = RT_get_adr(comnode, srvport, "MBXIN");
	mbx_out  = RT_get_adr(comnode, srvport, "MBXOUT");

	for (i = 0; i < NUM_TASKS; ++i) {
		sems[i] = RT_get_adr(comnode, srvport, sem[i]);
	}
printf("START TASK REL INIT COMNODE PORT %lx, %d\n", comnode, srvport);
	rt_make_soft_real_time();
	rt_release_port(comnode, srvport);
	thread = rt_thread_create(start_task_code, NULL, 10000);
	return 0;
}

int main(int argc, char *argv[])
{
	RT_TASK *task;
	struct sockaddr_in addr;
	int i, srvport;

	if (!(task = rt_task_init(nam2num("STRTSK"), 0, 0, 0))) {
		printf("CANNOT INIT START_TASK TASK\n");
		exit(1);
	}

	comnode = tasknode = 0;
	for (i = 0; i < argc; i++) {
		if (strstr(argv[i], "ComNode=")) {
			inet_aton(argv[i] + 8, &addr.sin_addr);
			comnode = addr.sin_addr.s_addr;
			argv[i] = 0;
			continue;
		}
		if (strstr(argv[i], "TaskNode=")) {
			inet_aton(argv[i] + 9, &addr.sin_addr);
			tasknode = addr.sin_addr.s_addr;
			argv[i] = 0;
			continue;
		}
	}
	if (!comnode) {
		inet_aton("127.0.0.1", &addr.sin_addr);
		comnode = addr.sin_addr.s_addr;
	}
	if (!tasknode) {
		inet_aton("127.0.0.1", &addr.sin_addr);
		tasknode = addr.sin_addr.s_addr;
	}
	rt_grow_and_lock_stack(100000);
	init_module();
	rt_thread_join(thread);
	while ((srvport = rt_request_hard_port(comnode)) <= 0) {
		msleep(100);
	}
	rt_make_hard_real_time();
	RT_sem_signal(comnode, srvport, end_sem);
	rt_make_soft_real_time();
	rt_release_port(comnode, srvport);
	rt_task_delete(task);
	exit(0);
}
