/*
COPYRIGHT (C) 2000  Paolo Mantegazza <mantegazza@aero.polimi.it>

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

#include <linux/module.h>
#include <asm/io.h>
#include <rtai_sched.h>
#include <rtai_sem.h>

MODULE_LICENSE("GPL");

#define TICK 1000000

#define RT_STACK 4000

static RT_TASK task1, task2, task3, task4;

static CND cond;

static SEM mtx;

static int cond_data;

static atomic_t cleanup = ATOMIC_INIT(0);

static void task_func1(long dummy)
{
	rt_printk("Starting task1, waiting on the conditional variable to be 1.\n");
	rt_mutex_lock(&mtx);
	while(cond_data < 1) {
		rt_cond_wait(&cond, &mtx);
	}
	rt_mutex_unlock(&mtx);
	if(cond_data == 1) {
		rt_printk("task1, conditional variable signalled, value: %d.\n", cond_data);
	}
	rt_printk("task1 signals after setting data to 2.\n");
	rt_printk("task1 waits for a broadcast.\n");
	rt_mutex_lock(&mtx);
	cond_data = 2;
	rt_cond_signal(&cond);
	while(cond_data < 3) {
		rt_cond_wait(&cond, &mtx);
	}
	rt_mutex_unlock(&mtx);
	if(cond_data == 3) {
		rt_printk("task1, conditional variable broadcasted, value: %d.\n", cond_data);
	}
	rt_printk("Ending task1.\n");
	atomic_inc(&cleanup);
}

static void task_func2(long dummy)
{
	rt_printk("Starting task2, waiting on the conditional variable to be 2.\n");
	rt_mutex_lock(&mtx);
	while(cond_data < 2) {
		rt_cond_wait(&cond, &mtx);
	}
	rt_mutex_unlock(&mtx);
	if(cond_data == 2) {
		rt_printk("task2, conditional variable signalled, value: %d.\n", cond_data);
	}
	rt_printk("task2 waits for a broadcast.\n");
	rt_mutex_lock(&mtx);
	while(cond_data < 3) {
		rt_cond_wait(&cond, &mtx);
	}
	rt_mutex_unlock(&mtx);
	if(cond_data == 3) {
		rt_printk("task2, conditional variable broadcasted, value: %d.\n", cond_data);
	}
	rt_printk("Ending task2.\n");
	atomic_inc(&cleanup);
}

static void task_func3(long dummy)
{
	rt_printk("Starting task3, waiting on the conditional variable to be 3 with a 2 s timeout.\n");
	rt_mutex_lock(&mtx);
	while(cond_data < 3) {
		if (rt_cond_timedwait(&cond, &mtx, rt_get_time() + nano2count(2000000000LL)) < 0) {
			break;
		}
	}
	rt_mutex_unlock(&mtx);
	if(cond_data < 3) {
		rt_printk("task3, timed out, conditional variable value: %d.\n", cond_data);
	}
	rt_mutex_lock(&mtx);
	cond_data = 3;
	rt_mutex_unlock(&mtx);
	rt_printk("task3 broadcasts after setting data to 3.\n");
	rt_cond_broadcast(&cond);
	rt_printk("Ending task3.\n");
	atomic_inc(&cleanup);
}

static void task_func4(long dummy)
{
	rt_printk("Starting task4, signalling after setting data to 1, then waits for a broadcast.\n");
	rt_mutex_lock(&mtx);
	cond_data = 1;
  	rt_mutex_unlock(&mtx);
	rt_cond_signal(&cond);
	rt_mutex_lock(&mtx);
	while(cond_data < 3) {
		rt_cond_wait(&cond, &mtx);
	}
	rt_mutex_unlock(&mtx);
	if(cond_data == 3) {
		rt_printk("task4, conditional variable broadcasted, value: %d.\n", cond_data);
	}
	rt_printk("Ending task4.\n");
	atomic_inc(&cleanup);
}

int init_module(void)
{
	printk("Conditional semaphore test program.\n");
	printk("Wait for all tasks to end, then type: ./rem.\n\n");
	start_rt_timer(nano2count(TICK));
	rt_cond_init(&cond);
	rt_mutex_init(&mtx);
	rt_task_init(&task1, task_func1, 0, RT_STACK, 0, 0, 0);
	rt_task_init(&task2, task_func2, 0, RT_STACK, 1, 0, 0);
	rt_task_init(&task3, task_func3, 0, RT_STACK, 2, 0, 0);
	rt_task_init(&task4, task_func4, 0, RT_STACK, 3, 0, 0);
	rt_task_resume(&task1);
	rt_task_resume(&task2);
	rt_task_resume(&task3);
	rt_task_resume(&task4);
	printk("Do not panic, wait 2 s, till task3 times out.\n\n");
	return 0;
}

void cleanup_module(void)
{
	while (atomic_read(&cleanup) < 4) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(HZ/10);
	}
	rt_cond_destroy(&cond);
	rt_mutex_destroy(&mtx);
	stop_rt_timer();
	rt_task_delete(&task1);
	rt_task_delete(&task2);
	rt_task_delete(&task3);
	rt_task_delete(&task4);
	printk("Conditional semaphore test program removed.\n");

}
