/*
COPYRIGHT (C) 2002  Wolfgang Grandegger (wg@denx)

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


/*
 * Simple SHM demo program: The application program "main.c" lists
 * a counter incremented in the RTAI kernel modue "ex_shm.c".
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_shm.h>
#include <rtai_nam2num.h>

MODULE_DESCRIPTION("Simple SHM demo");
MODULE_AUTHOR("Wolfgang Grandegger <wg@denx.de>");
MODULE_LICENSE("GPL");

#define STACK_SIZE    4000
#define TICK_PERIOD   100000
#define PERIOD_COUNT  1

#define MEGS    4
#define SHMNAM  "MYSHM"
#define SHMSIZ  MEGS*1024*1024

#undef SHM_DEBUG

static RT_TASK thread;

static int *shm;

static void fun(long t)
{
	int *p = shm, counter = 0;
	while (1) {
		if ((*p++ = ++counter) == (SHMSIZ - 2*sizeof(int))/sizeof(int)) {
			p = shm; counter = 0;
		}
		*shm = counter;
#ifdef SHM_DEBUG
		rt_printk("counter=%d\n", counter);
#endif
		rt_task_wait_period();
	}
}

int init_module (void)
{
	RTIME tick_period;
	RTIME now;
#ifdef SHM_DEBUG
	int size = SHMSIZ ;
	unsigned long vaddr, paddr;
#endif
	shm = (int *)rtai_kmalloc(nam2num(SHMNAM), SHMSIZ);
	shm = (int *)rtai_kmalloc(nam2num(SHMNAM), SHMSIZ);
	shm = (int *)rtai_kmalloc(nam2num(SHMNAM), SHMSIZ);
	shm = (int *)rtai_kmalloc(nam2num(SHMNAM), SHMSIZ);
	if (shm == NULL)
		return -ENOMEM;
	memset(shm, 0, SHMSIZ);
#ifdef SHM_DEBUG
	/* Show physical addresses of */
	vaddr = (unsigned long)shm;
	while (size > 0)
	{
		paddr = kvirt_to_pa(vaddr);
		printk("vaddr=0x%lx paddr=0x%lx\n", vaddr, paddr);
		vaddr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
#endif

	rt_task_init(&thread, fun, 0, STACK_SIZE, 0, 0, 0);
	tick_period = start_rt_timer(nano2count(TICK_PERIOD));
	now = rt_get_time();
	rt_task_make_periodic(&thread, now + tick_period, tick_period*PERIOD_COUNT);

	return 0;
}

void cleanup_module (void)
{
	stop_rt_timer();
	rt_busy_sleep(10000000);
	rt_task_delete(&thread);
	rtai_kfree(nam2num(SHMNAM));
	rtai_kfree(nam2num(SHMNAM));
	rtai_kfree(nam2num(SHMNAM));
	rtai_kfree(nam2num(SHMNAM));
	rtai_kfree(nam2num(SHMNAM));
	rtai_kfree(nam2num(SHMNAM));
}
