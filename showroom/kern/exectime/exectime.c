/*
COPYRIGHT (C) 2012  Shahbaz Youssefi (ShabbyX@gmail.com)

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
#include <linux/kernel.h>

#include <rtai.h>
#include <rtai_sched.h>

#define TICK_PERIOD	100000
#define PRIORITY	10
#define STACK_SIZE	4096

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Test correctness of rt_get_exectime");
MODULE_AUTHOR("Shahbaz Youssefi <ShabbyX@gmail.com>");

static RT_TASK task;
static char task_created = 0;

static void func(long int param)
{
	int times = 10;
	while (times--)
	{
		int i;
		RTIME t1 = 0, t2 = 0, e1 = 0, e2 = 0;
		RTIME start[3], end[3];

		t1 = rt_get_time_ns();
		e1 = task.exectime[0];
		rt_get_exectime(NULL, start);

		/* do some random stuff to avoid optimization */
		for (i = 0; i < 1000000; ++i)
			t2 += t1 + e1;

		t2 = rt_get_time_ns();
		rt_get_exectime(NULL, end);
		rt_printk("This value could be unupdated before wait_period: count2nano(e1'-e1): %llu\n",
				(unsigned long long)count2nano(task.exectime[0] - e1));

		rt_task_wait_period();

		e2 = task.exectime[0];
		rt_printk("t2-t1 (time difference using rt_get_time): %llu\n",
				(unsigned long long)(t2 - t1));
		rt_printk("count2nano(e2-e1) (time difference using task.exectime directly): %llu\n",
				(unsigned long long)count2nano(e2 - e1));
		rt_printk("count2nano(end[0]-start[0]) (time difference using rt_get_exectime): %llu\n",
				(unsigned long long)count2nano(end[0] - start[0]));
	}
}

static int exectime_init(void)
{
	int ret;

	rt_set_oneshot_mode();
	start_rt_timer(nano2count(TICK_PERIOD));
	if ((ret = rt_task_init(&task, func, 0, STACK_SIZE, PRIORITY, 0, NULL)))
		printk(KERN_INFO"Error creating task: %d (EINVAL: %d, ENOMEM: %d)\n", ret, (int)EINVAL, (int)ENOMEM);
	else
	{
		printk(KERN_INFO"Successfully created task\n");
		task_created = 1;
	}
	if (task_created && rt_task_make_periodic_relative_ns(&task, 100000, 25000000))
	{
		printk(KERN_INFO"Error making task periodic\n");
		rt_task_delete(&task);
		task_created = 0;
	}
	return 0;
}

static void exectime_exit(void)
{
	stop_rt_timer();
	if (task_created)
		rt_task_delete(&task);
}

module_init(exectime_init);
module_exit(exectime_exit);
