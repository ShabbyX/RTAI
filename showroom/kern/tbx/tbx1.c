/*
 * COPYRIGHT (C) 2001  G.M. Bertani <gmbertani@yahoo.it>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 *
 */

#include <linux/module.h>

#include <asm/io.h>

#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_tbx.h>

static RT_TASK t1, t2, t3, t4, t5, t6, t7;

static TBX bx;

static unsigned long count = 0;

static char msg1[] = {"MESSAGE FROM TASK1"};
static char msg2[] = {"BROADCAST BY TASK4"};
static char msg3[] = {"URGENT FROM TASK7!"};

#define BASE_PRIO 100
#define TBXSIZE (5*sizeof(msg1))  // big tbx stress
#define ORDER FIFO_Q              // FIFO ordered
#define MAXCOUNT 20
#define TIMEBASE 10000000
#define DELAY (20*TIMEBASE)
#define SHIFT (10*TIMEBASE)

static int cleanup;

static void Task1(long t)
{
	int unsent;

	rt_printk("\nTask1 (%p) starting\n", rt_whoami());
	while (count < MAXCOUNT) {
		count++;
		unsent = rt_tbx_send(&bx, msg1, sizeof(msg1));
		rt_printk("\nTask1=%lu, simple sending, unsent=%d %d\n", count, unsent, sizeof(msg1));
		rt_sleep(nano2count(DELAY));
	}
	rt_printk("\nTask1 suspends itself\n");
	rt_task_suspend(rt_whoami());
}

static void Task2(long t)
{
	int status;
	char buf[100];

	rt_printk("\nTask2 (%p) starting\n", rt_whoami());
	memset(buf, 'X', sizeof(buf));
	while (1) {
		status = rt_tbx_receive(&bx, buf, sizeof(msg1));
		rt_printk("\nTask2 received %s, status=%d\n", buf, status);
		memset(buf, 0, sizeof(buf));
	}
}

static void Task3(long t)
{
	int status;
	char buf[100];

	rt_printk("\nTask3 (%p) starting\n", rt_whoami());
	while (1) {
		status = rt_tbx_receive(&bx, buf, sizeof(msg1));
		rt_printk("\nTask3 received %s, status=%d\n", buf, status);
		memset(buf, 0, sizeof(buf));
	}
}

static void Task4(long t)
{
	int wakedup;

	rt_printk("\nTask4 (%p) starting\n", rt_whoami());
	while (count < MAXCOUNT) {
		count++;
		wakedup = rt_tbx_broadcast(&bx, msg2, sizeof(msg2));
		rt_printk("\nTask4=%lu, sending broadcast, wakedup=%d\n", count, wakedup);
		rt_sleep(nano2count(DELAY + SHIFT));
	}
	rt_printk("\nTask4 suspends itself\n");
	rt_task_suspend(rt_whoami());
}

static void Task5(long t)
{
	int status;
	char buf[100];

	rt_printk("\nTask5 (%p) starting\n", rt_whoami());
	while (1) {
		status = rt_tbx_receive(&bx, buf, sizeof(msg1));
		rt_printk("\nTask5 received %s, status=%d\n", buf, status);
		memset(buf, 0, sizeof(buf));
	}
}

static void Task6(long t)
{
	int status;
	char buf[100];

	rt_printk("\nTask6 (%p) starting\n", rt_whoami());
	while (1) {
		status = rt_tbx_receive(&bx, buf, sizeof(msg1));
		rt_printk("\nTask6 received %s, status=%d\n", buf, status);
		memset(buf, 0, sizeof(buf));
	}
}

static void Task7(long t)
{
	int unsent;

	rt_printk("\nTask7 (%p) starting\n", rt_whoami());
	while (count < MAXCOUNT) {
		count++;
		unsent = rt_tbx_urgent(&bx, msg3, sizeof(msg3));
		rt_printk("\nTask7=%lu, urgent sending, unsent=%d\n", count, unsent);
		rt_sleep(nano2count(3*DELAY + 2*SHIFT));
	}
	rt_printk("\nTask7 suspends itself\n");
	cleanup = 1;
	rt_task_suspend(rt_whoami());
}

int init_module(void)
{
	rt_task_init(&t1, Task1, 0, 2000, BASE_PRIO + 1, 0, 0);
	rt_task_init(&t2, Task2, 0, 2000, BASE_PRIO + 3, 0, 0);
	rt_task_init(&t3, Task3, 0, 2000, BASE_PRIO + 2, 0, 0);
	rt_task_init(&t4, Task4, 0, 2000, BASE_PRIO + 6, 0, 0);
	rt_task_init(&t5, Task5, 0, 2000, BASE_PRIO + 4, 0, 0);
	rt_task_init(&t6, Task6, 0, 2000, BASE_PRIO + 5, 0, 0);
	rt_task_init(&t7, Task7, 0, 2000, BASE_PRIO + 0, 0, 0);
	rt_tbx_init(&bx, TBXSIZE, ORDER);
	start_rt_timer(nano2count(TIMEBASE));
	rt_task_resume(&t1);
	rt_task_resume(&t2);
	rt_task_resume(&t3);
	rt_task_resume(&t4);
	rt_task_resume(&t5);
	rt_task_resume(&t6);
	rt_task_resume(&t7);
	rt_printk("\ntasks started\n");
	return 0;
}

void cleanup_module(void)
{
	while (!cleanup) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(HZ/10);
	}
	stop_rt_timer();
	rt_task_delete(&t1);
	rt_task_delete(&t2);
	rt_task_delete(&t3);
	rt_task_delete(&t4);
	rt_task_delete(&t5);
	rt_task_delete(&t6);
	rt_task_delete(&t7);
	rt_tbx_delete(&bx);
	rt_printk("\n\nEND\n\n");
}
