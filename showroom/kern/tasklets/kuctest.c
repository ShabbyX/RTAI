/*
COPYRIGHT (C) 2000  Paolo Mantegazza (mantegazza@aero.polimi.it)

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
#include <linux/version.h>
#include <linux/sched.h>

#include <rtai_lxrt.h>
#include <rtai_tasklets.h>
#include <rtai_shm.h>
#include "ctest.h"

static struct rt_tasklet_struct pt;

static int ptloops;

static struct mymem *mem;

static double s;

void ph(unsigned long data)
{
	int i;
	if (ptloops++ < LOOPS) {
		rt_sched_lock();
		s = 0.0;
		for (i = 0; i < SHMSIZ; i++) {
			s += mem->a[i];
		}
		rt_printk("\n%d FOUND: %lu", ptloops, (unsigned long)s);
		for (i = 0; i < SHMSIZ; i++) {
			mem->a[i] = 0;
		}
		rt_sched_unlock();
	} else {
		rt_printk("\n");
		rt_remove_timer(&pt);
		mem->flag = 1;
	}
}

int init_module(void)
{
        mem = rtai_kmalloc(0xabcd, sizeof(struct mymem));
	rt_insert_timer(&pt, 1, rt_get_time(), nano2count(TICK_PERIOD), ph, 0, 0);
	return 0;
}


void cleanup_module(void)
{
        rtai_kfree(0xabcd);
}
