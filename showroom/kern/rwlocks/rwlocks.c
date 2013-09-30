/*
COPYRIGHT (C) 2003  Paolo Mantegazza <mantegazza@aero.polimi.it>

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

/* RTAI rwlocks test */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_rwl.h>

MODULE_LICENSE("GPL");

#define STACK_SIZE  3000

#define LOOPS 1

#define NTASKS 6

static RT_TASK *thread;

static RWL rwl;

static int extcnt[NTASKS];

static void fun(long idx)
{
	unsigned int loops = LOOPS;
	while(loops--) {
		rt_printk("TASK %d 1 COND/TIMED PREWLOCKED\n", idx);
		if (idx%2) {
			if (rt_rwl_wrlock_if(&rwl)) {
				rt_printk("TASK %d 1 COND PREWLOCKED FAILED GO UNCOND\n", idx);
				rt_rwl_wrlock(&rwl);
			}
		} else if (rt_rwl_wrlock_timed(&rwl, nano2count(20000)) >= SEM_TIMOUT) {
			rt_printk("TASK %d 1 TIMED PREWLOCKED FAILED GO UNCOND\n", idx);
			rt_rwl_wrlock(&rwl);
		}
		rt_printk("TASK %d 1 WLOCKED\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 2 COND PREWLOCK\n", idx);
		if (rt_rwl_wrlock_if(&rwl)) {
			rt_printk("TASK %d 2 COND PREWLOCK FAILED GO UNCOND\n", idx);
			rt_rwl_wrlock(&rwl);
		}
		rt_printk("TASK %d 2 WLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 3 PREWLOCK\n", idx);
		rt_rwl_wrlock(&rwl);
		rt_printk("TASK %d 3 WLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 3 PREWUNLOCK\n", idx);
		rt_rwl_unlock(&rwl);
		rt_printk("TASK %d 3 WUNLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 2 PREWUNLOCK\n", idx);
		rt_rwl_unlock(&rwl);
		rt_printk("TASK %d 2 WUNLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 1 PREWUNLOCKED\n", idx);
		rt_rwl_unlock(&rwl);
		rt_printk("TASK %d 1 WUNLOCKED\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 1 COND/TIMED PRERDLOCKED\n", idx);
		if (idx%2) {
			if (rt_rwl_rdlock_if(&rwl)) {
				rt_printk("TASK %d 1 COND PRERDLOCKED FAILED GO UNCOND\n", idx);
				rt_rwl_rdlock(&rwl);
			}
		} else if (rt_rwl_rdlock_timed(&rwl, nano2count(20000)) >= SEM_TIMOUT) {
			rt_printk("TASK %d 1 TIMED PRERDLOCKED FAILED GO UNCOND\n", idx);
			rt_rwl_rdlock(&rwl);
		}
		rt_printk("TASK %d 1 RDLOCKED\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 2 COND PRERDLOCK\n", idx);
		if (rt_rwl_rdlock_if(&rwl)) {
			rt_printk("TASK %d 2 COND PRERDLOCK FAILED GO UNCOND\n", idx);
			rt_rwl_rdlock(&rwl);
		}
		rt_printk("TASK %d 2 RDLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 3 PRERDLOCK\n", idx);
		rt_rwl_rdlock(&rwl);
		rt_printk("TASK %d 3 RDLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 3 PRERDUNLOCK\n", idx);
		rt_rwl_unlock(&rwl);
		rt_printk("TASK %d 3 RDUNLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 2 PRERDUNLOCK\n", idx);
		rt_rwl_unlock(&rwl);
		rt_printk("TASK %d 2 RDUNLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 1 PRERDUNLOCK\n", idx);
		rt_rwl_unlock(&rwl);
		rt_printk("TASK %d 1 RDUNLOCK\n", idx);
		rt_busy_sleep(100000);
	}
	extcnt[idx - 1] = loops;
	rt_sleep(nano2count(10000000));
	rt_printk("TASK %d EXITED\n", idx);
}

int init_module(void)
{
	int i;

	thread = (RT_TASK *)kmalloc(NTASKS*sizeof(RT_TASK), GFP_KERNEL);
	rt_rwl_init(&rwl);
	rt_set_oneshot_mode();
	start_rt_timer(0);
	for (i = 0; i < NTASKS; i++) {
		extcnt[i] = 999999;
		rt_task_init(&thread[i], fun, i + 1, STACK_SIZE, NTASKS - i, 0, 0);
		rt_task_resume(&thread[i]);
	}
	return 0;
}

void cleanup_module(void)
{
	int i;

	stop_rt_timer();
	rt_rwl_delete(&rwl);
	for (i = 0; i < NTASKS; i++) {
		printk("TASK %d LOOPS COUNT AT EXIT %d\n", i + 1, extcnt[i]);
		rt_task_delete(&thread[i]);
	}
	kfree(thread);
}
