/*
COPYRIGHT (C) 2003  Paolo Mantegazza (mantegazza@aero.polimi.it)

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


/* RTAI spinlocks test */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_spl.h>

MODULE_LICENSE("GPL");

#define STACK_SIZE  3000

#define LOOPS 1

#define NTASKS 6

static RT_TASK *thread;

static SPL spl;

static int extcnt[NTASKS];

static void fun(long idx)
{
	unsigned int loops = LOOPS;
	while(loops--) {
		rt_printk("TASK %d 1 COND/TIMED PRELOCKED\n", idx);
		if (idx%2) {
			if (rt_spl_lock_if(&spl)) {
				rt_printk("TASK %d 1 COND PRELOCKED FAILED GO UNCOND\n", idx);
				rt_spl_lock(&spl);
			}
		} else if (rt_spl_lock_timed(&spl, nano2count(20000))) {
			rt_printk("TASK %d 1 TIMED PRELOCKED FAILED GO UNCOND\n", idx);
			rt_spl_lock(&spl);
		}
		rt_printk("TASK %d 1 LOCKED\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 2 COND PRELOCK\n", idx);
		if (rt_spl_lock_if(&spl)) {
			rt_printk("TASK %d 2 COND PRELOCK FAILED GO UNCOND\n", idx);
			rt_spl_lock(&spl);
		}
		rt_printk("TASK %d 2 LOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 3 PRELOCK\n", idx);
		rt_spl_lock(&spl);
		rt_printk("TASK %d 3 LOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 3 PREUNLOCK\n", idx);
		rt_spl_unlock(&spl);
		rt_printk("TASK %d 3 UNLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 2 PREUNLOCK\n", idx);
		rt_spl_unlock(&spl);
		rt_printk("TASK %d 2 UNLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 1 PREUNLOCKED\n", idx);
		rt_spl_unlock(&spl);
		rt_printk("TASK %d 1 UNLOCKED\n", idx);
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
	rt_spl_init(&spl);
	rt_set_oneshot_mode();
	start_rt_timer(0);
	for (i = 0; i < NTASKS; i++) {
		extcnt[i] = 999999;
		rt_task_init(&thread[i], fun, i + 1, STACK_SIZE, 0, 0, 0);
		rt_task_resume(&thread[i]);
	}
	return 0;
}


void cleanup_module(void)
{
	int i;

	stop_rt_timer();
	rt_spl_delete(&spl);
	for (i = 0; i < NTASKS; i++) {
		printk("TASK %d LOOPS COUNT AT EXIT %d\n", i + 1, extcnt[i]);
		rt_task_delete(&thread[i]);
	}
	kfree(thread);
}
