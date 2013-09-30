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

#include <rtai_posix.h>

MODULE_LICENSE("GPL");

#define LOOPS 1

#define NTASKS 4

static pthread_t *thread;

static pthread_rwlock_t rwl;

static int extcnt[NTASKS], cleanup;

static void *fun(int idx)
{
	unsigned int loops = LOOPS;
	while(loops--) {
		rt_printk("TASK %d 1 COND/TIMED PREWLOCKED\n", idx);
		if (idx%2) {
			if (pthread_rwlock_trywrlock(&rwl)) {
				rt_printk("TASK %d 1 COND PREWLOCKED FAILED GO UNCOND\n", idx);
				pthread_rwlock_wrlock(&rwl);
			}
		} else {
			struct timespec abstime;
			abstime.tv_sec = llimd(rt_get_cpu_time_ns(), 1, 1000000000);
			abstime.tv_nsec = 20000;
			if (pthread_rwlock_timedwrlock(&rwl, &abstime) < 0) {
				rt_printk("TASK %d 1 TIMED PREWLOCKED FAILED GO UNCOND\n", idx);
				pthread_rwlock_wrlock(&rwl);
			}
		}
		rt_printk("TASK %d 1 WLOCKED\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 2 COND PREWLOCK\n", idx);
		if (pthread_rwlock_trywrlock(&rwl)) {
			rt_printk("TASK %d 2 COND PREWLOCK FAILED GO UNCOND\n", idx);
			pthread_rwlock_wrlock(&rwl);
		}
		rt_printk("TASK %d 2 WLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 3 PREWLOCK\n", idx);
		pthread_rwlock_wrlock(&rwl);
		rt_printk("TASK %d 3 WLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 3 PREWUNLOCK\n", idx);
		pthread_rwlock_unlock(&rwl);
		rt_printk("TASK %d 3 WUNLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 2 PREWUNLOCK\n", idx);
		pthread_rwlock_unlock(&rwl);
		rt_printk("TASK %d 2 WUNLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 1 PREWUNLOCKED\n", idx);
		pthread_rwlock_unlock(&rwl);
		rt_printk("TASK %d 1 WUNLOCKED\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 1 COND/TIMED PRERDLOCKED\n", idx);
		if (idx%2) {
			if (pthread_rwlock_tryrdlock(&rwl)) {
				rt_printk("TASK %d 1 COND PRERDLOCKED FAILED GO UNCOND\n", idx);
				pthread_rwlock_rdlock(&rwl);
			}
		} else {
			struct timespec abstime;
			abstime.tv_sec = llimd(rt_get_cpu_time_ns(), 1, 1000000000);
			abstime.tv_nsec = 20000;
			if (pthread_rwlock_timedrdlock(&rwl, &abstime) < 0) {
				rt_printk("TASK %d 1 TIMED PRERDLOCKED FAILED GO UNCOND\n", idx);
				pthread_rwlock_rdlock(&rwl);
			}
		}
		rt_printk("TASK %d 1 RDLOCKED\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 2 COND PRERDLOCK\n", idx);
		if (pthread_rwlock_tryrdlock(&rwl)) {
			rt_printk("TASK %d 2 COND PRERDLOCK FAILED GO UNCOND\n", idx);
			pthread_rwlock_rdlock(&rwl);
		}
		rt_printk("TASK %d 2 RDLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 3 PRERDLOCK\n", idx);
		pthread_rwlock_rdlock(&rwl);
		rt_printk("TASK %d 3 RDLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 3 PRERDUNLOCK\n", idx);
		pthread_rwlock_unlock(&rwl);
		rt_printk("TASK %d 3 RDUNLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 2 PRERDUNLOCK\n", idx);
		pthread_rwlock_unlock(&rwl);
		rt_printk("TASK %d 2 RDUNLOCK\n", idx);
		rt_busy_sleep(100000);
		rt_printk("TASK %d 1 PRERDUNLOCK\n", idx);
		pthread_rwlock_unlock(&rwl);
		rt_printk("TASK %d 1 RDUNLOCK\n", idx);
		rt_busy_sleep(100000);
	}
	extcnt[idx - 1] = loops;
	rt_sleep(nano2count(10000000));
	rt_printk("TASK %d EXITED\n", idx);
	cleanup = 1;
	return 0;
}

int init_module(void)
{
	int i;
	pthread_attr_t attr = { STACK_SIZE, 0, 0, 0 };

	thread = (pthread_t *)kmalloc(NTASKS*sizeof(pthread_t), GFP_KERNEL);
	pthread_rwlock_init(&rwl, 0);
	rt_set_oneshot_mode();
	start_rt_timer(0);
	for (i = 0; i < NTASKS; i++) {
		extcnt[i] = 999999;
		attr.priority = NTASKS - i;
		pthread_create(&thread[i], &attr, (void *)fun, (void *)(i + 1));
	}
	while (!cleanup) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(HZ/10);
	}
	return 0;
}

void cleanup_module(void)
{
	int i;

	stop_rt_timer();
	pthread_rwlock_destroy(&rwl);
	for (i = 0; i < NTASKS; i++) {
		printk("TASK %d LOOPS COUNT AT EXIT %d\n", i + 1, extcnt[i]);
		pthread_cancel(thread[i]);
	}
	kfree(thread);
}
