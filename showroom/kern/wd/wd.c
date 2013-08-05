/*
COPYRIGHT (C) 2007  Paolo Mantegazza <mantegazza@aero.polimi.it>

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


#include <linux/kernel.h>
#include <linux/module.h>

#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_sem.h>
#include <rtai_msg.h>

MODULE_LICENSE("GPL");

static RT_TASK *task;

#define PERIOD 1000000 // ns

static void fun(long usemsg)
{
	if (rt_is_hard_timer_running()) {
		rt_task_make_periodic(task, rt_get_time(), nano2count(PERIOD));
	}
	if (usemsg) {
		unsigned long msg;
		while (!rt_receive_if(NULL, &msg));
	} else {
		SEM *sem;
		sem = rt_typed_named_sem_init("KILSEM", 0, CNT_SEM);
		while (rt_sem_wait_if(sem) <= 0);
		rt_named_sem_delete(sem);
	}
}

int init_module(void)
{
	task = rt_named_task_init_cpuid("LOOPER", fun, 1, 8000, 1, 0, 0, 1);
	rt_task_resume(task);
	return 0;
}

void cleanup_module(void)
{
	rt_named_task_delete(task);
}
