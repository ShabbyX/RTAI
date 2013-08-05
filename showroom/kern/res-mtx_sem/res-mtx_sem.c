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

/* An adaption of the same test for posix mutexes, by Steve Papacharalambous */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>

#include <asm/rtai.h>
#include <rtai_sched.h>
#include <rtai_sem.h>

#define RT_STACK 2000

#define DEFAULT_TICK_PERIOD 1000000

#define T_BUFFER_SIZE 256

#define NUM_TASKS 10

#define NESTING 20

#define TID 1000

//#define MUTEX_LOCK  rt_sem_wait(&test_op.lock)
#define MUTEX_LOCK  rt_sem_wait_timed(&test_op.lock, 2000000000)

#define RT_PRINTK rt_printk

struct op_buffer {
	int t_buffer_data[T_BUFFER_SIZE];
	long t_buffer_tid[T_BUFFER_SIZE];
	int t_buffer_pos;
	SEM lock;
};

static struct op_buffer test_op;

void task_func(long tid)
{
	RT_TASK *task;
	int i;

	task = rt_whoami();
  	RT_PRINTK("task: %d, priority: %d\n", tid, task->priority);
	for (i = 0; i < NESTING; i++) {
		MUTEX_LOCK;
	}
	for (i = 0; i < 5; i++) {
		RT_PRINTK("task: %d, loop count %d\n", tid, i);
		test_op.t_buffer_data[test_op.t_buffer_pos] = tid + i;
		test_op.t_buffer_tid[test_op.t_buffer_pos] = tid;
		test_op.t_buffer_pos++;
	}
  	RT_PRINTK("task: %d, priority raised to: %d (nesting: %d)\n", tid, task->priority, test_op.lock.type);
	for (i = 0; i < (NESTING - 1); i++) {
		rt_sem_signal(&test_op.lock);
	}
	RT_PRINTK("task: %d, about to unlock mutex (nesting left: %d).\n", tid, test_op.lock.type);
	rt_sem_signal(&test_op.lock);
	RT_PRINTK("task: %d, priority should now be back to original.\n", tid);
	RT_PRINTK("task: %d, priority back to %d.\n", tid, task->priority);
	rt_task_suspend(task);
}

static RT_TASK tasks[NUM_TASKS];

void control_func(long tid)
{
	int i;
	RT_TASK *task;

	task = rt_whoami();
	RT_PRINTK("task: %d, priority: %d\n", tid, task->priority);

	rt_typed_sem_init(&test_op.lock, 1, RES_SEM);
	test_op.t_buffer_pos = 0;
	for (i = 0; i < T_BUFFER_SIZE; i++) {
		test_op.t_buffer_data[i] = 0;
		test_op.t_buffer_tid[i] = 0;
	}
	for (i = 0; i < NUM_TASKS; i++) {
		if (rt_task_init(&tasks[i], task_func, TID + i + 1, RT_STACK, NUM_TASKS - i, 0, 0)) {
			RT_PRINTK("resource semaphore priority inheritance test - Application task creation failed\n");
		}
	}
	for (i = 0; i < NESTING; i++) {
		MUTEX_LOCK;
	}
	for (i = 0; i < NUM_TASKS; i++) {
		rt_task_resume(&tasks[i]);
		RT_PRINTK("task: %d, priority raised to: %d\n", tid, task->priority);
	}
	RT_PRINTK("task: %d, priority finally raised to: %d (nesting: %d)\n", tid, task->priority, test_op.lock.type);
	for (i = 0; i < (NESTING - 1); i++) {
		rt_sem_signal(&test_op.lock);
	}
	RT_PRINTK("task: %d, about to unlock mutex (nesting left: %d).\n", tid, test_op.lock.type);
	rt_sem_signal(&test_op.lock);
	RT_PRINTK("task: %d, priority should now be back to original.\n", tid);
	RT_PRINTK("task: %d, priority back to %d.\n", tid, task->priority);
	rt_task_suspend(task);
}

static RT_TASK control_task;

int init_module(void)
{
	start_rt_timer(nano2count(DEFAULT_TICK_PERIOD));
	printk("resource semaphores priority inheritance test program.\n");
// Create a control pthread.
	if (rt_task_init(&control_task, control_func, TID, RT_STACK, 1000, 0, 0)) {
		printk("resource semaphores priority inheritance test - Control task creation failed\n");
		return -1;
	}
	rt_task_resume(&control_task);
	return 0;
}

void cleanup_module(void)
{
	int i;
	stop_rt_timer();
	printk("resource semaphore test - Contents of test_op buffer:\n");
	for (i = 0; i < test_op.t_buffer_pos; i++) {
		printk("Position: %d, Value: %d, Task id: %ld\n", i + 1, test_op.t_buffer_data[i], test_op.t_buffer_tid[i]);
	}
	rt_task_delete(&control_task);
	for (i = 0; i < NUM_TASKS; i++) {
		rt_task_delete(&tasks[i]);
	}
	rt_sem_delete(&test_op.lock);
	printk("resource semaphores test module removed.\n");
}
