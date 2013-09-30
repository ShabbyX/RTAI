/*
COPYRIGHT (C) 2002  Paolo Mantegazza <mantegazza@aero.polimi.it>

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
#include <rtai_bits.h>

MODULE_LICENSE("GPL");

#define SLEEP_TIME  200000

#define STACK_SIZE  2000

static atomic_t cleanup;

static RT_TASK task0, task1, task;

static BITS bits;

static void fun0(long t)
{
	unsigned long resulting_mask;
	int timeout;
	rt_printk("FUN0 WAITS FOR ALL_SET TO 0x0000FFFF WHILE BITS MASK IS %lx\n", bits.mask);
	rt_bits_wait(&bits, ALL_SET, 0x0000FFFF, NOP_BITS, 0, &resulting_mask);
	rt_printk("FUN0 RESUMED WITH BITS MASK %lx\n", bits.mask);
	rt_sleep(nano2count(SLEEP_TIME));
	rt_printk("FUN0 WAITS_IF FOR ALL_SET TO 0xFFFF0000 WHILE BITS MASK IS %lx\n", bits.mask);
	rt_bits_wait_if(&bits, ALL_SET, 0xFFFF0000, NOP_BITS, 0, &resulting_mask);
	rt_printk("FUN0 WAITS FOR ALL_SET TO 0xFFFF0000 WHILE BITS MASK IS %lx\n", bits.mask);
	rt_bits_wait(&bits, ALL_SET, 0xFFFF0000, NOP_BITS, 0, &resulting_mask);
	rt_printk("FUN0 RESUMED WITH BITS MASK %lx\n", bits.mask);
	rt_sleep(nano2count(SLEEP_TIME));
	rt_printk("FUN0 WAITS 2 s FOR ALL_SET TO 0xFFFFFFFF WHILE BITS MASK IS %lx\n", bits.mask);
	timeout = rt_bits_wait_timed(&bits, ALL_SET, 0xFFFFFFFF, NOP_BITS, 0, nano2count(2000000000LL), &resulting_mask);
	rt_printk("FUN0 RESUMED WITH BITS MASK %lx %s\n", bits.mask, timeout == BITS_TIMOUT ? "(TIMEOUT)" : "\0");
	atomic_inc(&cleanup);
}

static void fun1(long t)
{
	unsigned long resulting_mask;
	int timeout;
	rt_printk("FUN1 WAITS FOR ALL_CLR TO 0xFFFF0000 WHILE BITS MASK IS %lx\n", bits.mask);
	rt_bits_wait(&bits, ALL_CLR, 0xFFFF0000, NOP_BITS, 0, &resulting_mask);
	rt_printk("FUN1 RESUMED WITH BITS MASK %lx\n", bits.mask);
	rt_sleep(nano2count(SLEEP_TIME));
	rt_printk("FUN1 WAITS_IF FOR ALL_SET TO 0xFFFF0000 WHILE BITS MASK IS %lx\n", bits.mask);
	rt_bits_wait_if(&bits, ALL_SET, 0xFFFF0000, NOP_BITS, 0, &resulting_mask);
	rt_printk("FUN1 WAITS FOR ALL_SET TO 0xFFFF0000 WHILE BITS MASK IS %lx\n", bits.mask);
	rt_bits_wait(&bits, ALL_SET, 0xFFFF0000, NOP_BITS, 0, &resulting_mask);
	rt_printk("FUN1 RESUMED WITH BITS MASK %lx\n", bits.mask);
	rt_sleep(nano2count(SLEEP_TIME));
	rt_printk("FUN1 WAITS 2 s FOR ALL_SET TO 0xFFFFFFFF WHILE BITS MASK IS %lx\n", bits.mask);
	timeout = rt_bits_wait_timed(&bits, ALL_SET, 0xFFFFFFFF, NOP_BITS, 0, nano2count(2000000000LL), &resulting_mask);
	rt_printk("FUN1 RESUMED WITH BITS MASK %lx %s\n", bits.mask, timeout == BITS_TIMOUT ? "(TIMEOUT)" : "\0");
	atomic_inc(&cleanup);
}

static void fun(long t)
{
	rt_printk("SIGNAL SET_BITS 0x0000FFFF AND RETURNS BITS MASK %lx\n",
	rt_bits_signal(&bits, SET_BITS, 0x0000FFFF));
	rt_sleep(nano2count(SLEEP_TIME));
	rt_printk("SIGNAL CLR_BITS 0xFFFF0000 AND RETURNS BITS MASK %lx\n",
	rt_bits_signal(&bits, CLR_BITS, 0xFFFF0000));
	rt_sleep(nano2count(4*SLEEP_TIME));
	rt_printk("RESET BITS TO 0\n");
	rt_bits_reset(&bits, 0);
	rt_sleep(nano2count(SLEEP_TIME));
	rt_bits_reset(&bits, 0);
	atomic_inc(&cleanup);
}

int init_module(void)
{
	rt_bits_init(&bits, 0xFFFF0000);
	rt_task_init(&task0, fun0, 0, STACK_SIZE, 0, 0, 0);
	rt_task_init(&task1, fun1, 0, STACK_SIZE, 0, 0, 0);
	rt_task_init(&task, fun, 0, STACK_SIZE, 0, 0, 0);
	rt_set_oneshot_mode();
	start_rt_timer(nano2count(SLEEP_TIME));
	rt_task_resume(&task0);
	rt_task_resume(&task1);
	rt_task_resume(&task);
	return 0;
}

void cleanup_module(void)
{
	while (atomic_read(&cleanup) < 3) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(HZ/10);
	}
	stop_rt_timer();
	rt_bits_delete(&bits);
	rt_task_delete(&task0);
	rt_task_delete(&task1);
	rt_task_delete(&task);
}
