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


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

#include <rtai_bits.h>

#define SLEEP_TIME  200000

static int thread0, thread1, thread;

static BITS *bits;

static void *fun0(void *arg)
{
	RT_TASK *task;
	unsigned long resulting_mask;
	int timeout;

	task = rt_task_init_schmod(nam2num("TASK0"), 0, 0, 0, SCHED_FIFO, 0xF);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();
	rt_printk("FUN0 WAITS FOR ALL_SET TO 0x0000FFFF WHILE BITS MASK IS %lx\n", rt_get_bits(bits));
	rt_bits_wait(bits, ALL_SET, 0x0000FFFF, NOP_BITS, 0, &resulting_mask);
	rt_printk("FUN0 RESUMED WITH BITS MASK %lx\n", rt_get_bits(bits));
	rt_sleep(nano2count(SLEEP_TIME));
	rt_printk("FUN0 WAITS_IF FOR ALL_SET TO 0xFFFF0000 WHILE BITS MASK IS %lx\n", rt_get_bits(bits));
	rt_bits_wait_if(bits, ALL_SET, 0xFFFF0000, NOP_BITS, 0, &resulting_mask);
	rt_printk("FUN0 WAITS FOR ALL_SET TO 0xFFFF0000 WHILE BITS MASK IS %lx\n", rt_get_bits(bits));
	rt_bits_wait(bits, ALL_SET, 0xFFFF0000, NOP_BITS, 0, &resulting_mask);
	rt_printk("FUN0 RESUMED WITH BITS MASK %lx\n", rt_get_bits(bits));
	rt_sleep(nano2count(SLEEP_TIME));
	rt_printk("FUN0 WAITS 2 s FOR ALL_SET TO 0xFFFFFFFF WHILE BITS MASK IS %lx\n", rt_get_bits(bits));
	timeout = rt_bits_wait_timed(bits, ALL_SET, 0xFFFFFFFF, NOP_BITS, 0, nano2count(2000000000LL), &resulting_mask);
	rt_printk("FUN0 RESUMED WITH BITS MASK %lx %s\n", rt_get_bits(bits), timeout == BITS_TIMOUT ? "(TIMEOUT)" : "\0");
	rt_make_soft_real_time();
	rt_task_delete(task);
	return 0;
}

static void *fun1(void *arg)
{
	RT_TASK *task;
	unsigned long resulting_mask;
	int timeout;

	task = rt_task_init_schmod(nam2num("TASK1"), 0, 0, 0, SCHED_FIFO, 0xF);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();
	rt_printk("FUN1 WAITS FOR ALL_CLR TO 0xFFFF0000 WHILE BITS MASK IS %lx\n", rt_get_bits(bits));
	rt_bits_wait(bits, ALL_CLR, 0xFFFF0000, NOP_BITS, 0, &resulting_mask);
	rt_printk("FUN1 RESUMED WITH BITS MASK %lx\n", rt_get_bits(bits));
	rt_sleep(nano2count(SLEEP_TIME));
	rt_printk("FUN1 WAITS_IF FOR ALL_SET TO 0xFFFF0000 WHILE BITS MASK IS %lx\n", rt_get_bits(bits));
	rt_bits_wait_if(bits, ALL_SET, 0xFFFF0000, NOP_BITS, 0, &resulting_mask);
	rt_printk("FUN1 WAITS FOR ALL_SET TO 0xFFFF0000 WHILE BITS MASK IS %lx\n", rt_get_bits(bits));
	rt_bits_wait(bits, ALL_SET, 0xFFFF0000, NOP_BITS, 0, &resulting_mask);
	rt_printk("FUN1 RESUMED WITH BITS MASK %lx\n", rt_get_bits(bits));
	rt_sleep(nano2count(SLEEP_TIME));
	rt_printk("FUN1 WAITS 2 s FOR ALL_SET TO 0xFFFFFFFF WHILE BITS MASK IS %lx\n", rt_get_bits(bits));
	timeout = rt_bits_wait_timed(bits, ALL_SET, 0xFFFFFFFF, NOP_BITS, 0, nano2count(2000000000LL), &resulting_mask);
	rt_printk("FUN1 RESUMED WITH BITS MASK %lx %s\n", rt_get_bits(bits), timeout == BITS_TIMOUT ? "(TIMEOUT)" : "\0");
	rt_make_soft_real_time();
	rt_task_delete(task);
	return 0;
}

static void *fun(void *arg)
{
	RT_TASK *task;

	task = rt_task_init_schmod(nam2num("TASK"), 0, 0, 0, SCHED_FIFO, 0xF);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();
	rt_printk("SIGNAL SET_BITS 0x0000FFFF AND RETURNS BITS MASK %lx\n",
	rt_bits_signal(bits, SET_BITS, 0x0000FFFF));
	rt_sleep(nano2count(SLEEP_TIME));
	rt_printk("SIGNAL CLR_BITS 0xFFFF0000 AND RETURNS BITS MASK %lx\n",
	rt_bits_signal(bits, CLR_BITS, 0xFFFF0000));
	rt_sleep(nano2count(SLEEP_TIME));
	rt_printk("RESET BITS TO 0\n");
	rt_bits_reset(bits, 0);
	rt_sleep(nano2count(SLEEP_TIME));
	rt_bits_reset(bits, 0);
	rt_make_soft_real_time();
	rt_task_delete(task);
	return 0;
}

int main(void)
{
	RT_TASK *task;

	task = rt_task_init_schmod(nam2num("MYTASK"), 9, 0, 0, SCHED_FIFO, 0xF);
	bits = rt_bits_init(nam2num("BITS"), 0xFFFF0000);
	rt_set_oneshot_mode();
	start_rt_timer(nano2count(SLEEP_TIME));
	thread0 = rt_thread_create(fun0, NULL, 10000);
	rt_sleep(nano2count(SLEEP_TIME));
	thread1 = rt_thread_create(fun1, NULL, 10000);
	rt_sleep(nano2count(SLEEP_TIME));
	thread = rt_thread_create(fun, NULL, 10000);
	printf("TYPE <ENTER> TO TERMINATE\n");
	getchar();
	stop_rt_timer();
	rt_bits_delete(bits);
	rt_task_delete(task);
	return 0;
}
