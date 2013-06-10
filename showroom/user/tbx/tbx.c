/*
 * COPYRIGHT (C) 2001  G.M. Bertani (gmbertani@yahoo.it)
 *               2003  P. Mantegazza (mantegazza@aero.polimi.it)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USAr.
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/io.h>
#include <math.h>

#include <rtai_tbx.h>


static TBX *bx;

static volatile unsigned long count, endall;

typedef struct {
	unsigned progressive;
	unsigned sending_task;
} MSG;

#define BASE_PRIO 1
#define TBXSIZE (sizeof(MSG) + 1)       //big tbx stress!
#define ORDER FIFO_Q                    //FIFO ordered
#define MAXCOUNT 20
#define TIMEBASE 10000000
#define DELAY (20*TIMEBASE)
#define SHIFT (10*TIMEBASE)

static void *Task1(void *arg)
{
	RT_TASK *t1;
	int unsent;
	MSG msg;

	t1 = rt_task_init_schmod(nam2num("T1"), BASE_PRIO + 1, 0, 0, SCHED_FIFO, 0xF);
	printf("\nTask1 (%p) starting\n", t1);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	while (count < MAXCOUNT) {
		count++;
		msg.progressive = count;
		msg.sending_task = 1;
		unsent = rt_tbx_send(bx, (char*)&msg, sizeof(msg));
		printf("\nTask1=%lu, simple sending, unsent=%d\n", count, unsent);
		rt_sleep(nano2count(DELAY));
	}
	rt_task_delete(t1);
	printf("\nTask1 ends itself\n");
	return 0;
}

static void *Task2(void *arg)
{
	RT_TASK *t2;
	int status;
	MSG buf;

	t2 = rt_task_init_schmod(nam2num("T2"), BASE_PRIO + 3, 0, 0, SCHED_FIFO, 0xF);
	printf("\nTask2 (%p) starting\n", t2);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	while (!endall) {
		memset((char*)&buf, 0, sizeof(buf));
		status = rt_tbx_receive(bx, (char*)&buf, sizeof(buf));
		printf("\nTask2 received msgnr %u from task %u, status=%d\n", buf.progressive, buf.sending_task, status);
	}
	printf("\nTask2 ends itself\n");
	return 0;
}

static void *Task3(void *arg)
{
	RT_TASK *t3;
	int status;
	MSG buf;

	t3 = rt_task_init_schmod(nam2num("T3"), BASE_PRIO + 2, 0, 0, SCHED_FIFO, 0xF);
	printf("\nTask3 (%p) starting\n", t3);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	while (!endall) {
		memset((char*)&buf, 0, sizeof(buf));
		status = rt_tbx_receive(bx, (char*)&buf, sizeof(buf));
		printf("\nTask3 received msgnr %u from task %u, status=%d\n", buf.progressive, buf.sending_task, status);

	}
	rt_task_delete(t3);
	printf("\nTask3 ends itself\n");
	return 0;
}

static void *Task4(void *arg)
{
	RT_TASK *t4;
	int wakedup;
	MSG msg;

	t4 = rt_task_init_schmod(nam2num("T4"), BASE_PRIO + 6, 0, 0, SCHED_FIFO, 0xF);
	printf("\nTask4 (%p) starting\n", t4);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	while (count < MAXCOUNT) {
		count++;
		msg.progressive = count;
		msg.sending_task = 4;
		wakedup = rt_tbx_broadcast(bx, (char*)&msg, sizeof(msg));
		printf("\nTask4=%lu, sent broadcast, wakedup=%d\n", count, wakedup);
		rt_sleep(nano2count(DELAY + SHIFT));
	}
	rt_task_delete(t4);
	printf("\nTask4 ends itself\n");
	return 0;
}

static void *Task5(void *arg)
{
	RT_TASK *t5;
	int status;
	MSG buf;

	t5 = rt_task_init_schmod(nam2num("T5"), BASE_PRIO + 4, 0, 0, SCHED_FIFO, 0xF);
	printf("\nTask5 (%p) starting\n", t5);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	while (!endall) {
		memset((char*)&buf, 0, sizeof(buf));
		status = rt_tbx_receive(bx, (char*)&buf, sizeof(buf));
		printf("\nTask5 received msgnr %u from task %u, status=%d\n", buf.progressive, buf.sending_task, status);
	}
	rt_task_delete(t5);
	printf("\nTask5 ends itself\n");
	return 0;
}

static void *Task6(void *arg)
{
	RT_TASK *t6;
	int status;
	MSG buf;

	t6 = rt_task_init_schmod(nam2num("T6"), BASE_PRIO + 5, 0, 0, SCHED_FIFO, 0xF);
	printf("\nTask6 (%p) starting\n", t6);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	while (!endall) {
		memset((char*)&buf, 0, sizeof(buf));
		status = rt_tbx_receive(bx, (char*)&buf, sizeof(buf));
		printf("\nTask6 received msgnr %u from task %u, status=%d\n", buf.progressive, buf.sending_task, status);
	}
	rt_task_delete(t6);
	printf("\nTask6 ends itself\n");
	return 0;
}

static void *Task7(void *arg)
{
	RT_TASK *t7;
	int unsent;
	MSG msg;

	t7 = rt_task_init_schmod(nam2num("T7"), BASE_PRIO + 0, 0, 0, SCHED_FIFO, 0xF);
	printf("\nTask7 (%p) starting\n", t7);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	while (count < MAXCOUNT) {
		count++;
		msg.progressive = count;
		msg.sending_task = 7;
		unsent = rt_tbx_urgent(bx, (char*)&msg, sizeof(msg));
		printf("\nTask7=%lu, urgent sending, unsent=%d\n", count, unsent);
		rt_sleep(nano2count(3*DELAY + 2*SHIFT));
	}
	rt_task_delete(t7);
	printf("\nTask7 ends itself\n");
	return 0;
}

static pthread_t pt1, pt2, pt3, pt4, pt5, pt6, pt7;

int main(void)
{
	RT_TASK *maint;
	MSG msg = { 0, 0 };

	maint = rt_task_init(nam2num("MAIN"), 99, 0, 0);
	bx = rt_tbx_init(nam2num("BX"), TBXSIZE, ORDER);
	start_rt_timer(nano2count(TIMEBASE));
	pthread_create(&pt1, NULL, Task1, NULL);
	pthread_create(&pt2, NULL, Task2, NULL);
	pthread_create(&pt3, NULL, Task3, NULL);
	pthread_create(&pt4, NULL, Task4, NULL);
	pthread_create(&pt5, NULL, Task5, NULL);
	pthread_create(&pt6, NULL, Task6, NULL);
	pthread_create(&pt7, NULL, Task7, NULL);
	printf("\ntasks started\n");
        pthread_join(pt1, NULL);
        pthread_join(pt4, NULL);
        pthread_join(pt7, NULL);
	endall = 1;
	rt_tbx_broadcast(bx, (char*)&msg, sizeof(msg));
        pthread_join(pt2, NULL);
        pthread_join(pt3, NULL);
        pthread_join(pt5, NULL);
        pthread_join(pt6, NULL);
	rt_tbx_delete(bx);
	rt_task_delete(maint);
	stop_rt_timer();
	printf("\n\nEND\n\n");
	return 0;
}
