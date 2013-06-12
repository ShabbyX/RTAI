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

#include <asm/rtai.h>
#include <rtai_shm.h>
#include <rtai_sched.h>
#include <rtai_lxrt.h>
#include <rtai_tasklets.h>
#include "ctest.h"

#define SUPRT USE_VMALLOC

#define ONE_SHOT

static struct mymem *mem;

static struct rt_tasklet_struct pt;

void ph(unsigned long data)
{
	int i;
	if (!mem->flag) {
		rt_sched_lock();
		for (i = 0; i < SHMSIZ; i++) {
			mem->a[i] = i + 1;
		}
		rt_sched_unlock();
	} else {
//		rt_remove_timer(&pt);
	}
}

int init_module(void)
{
	mem = rt_shm_alloc(0xabcd, sizeof(struct mymem), SUPRT);
	mem->flag = 0;
#ifdef ONE_SHOT
	rt_set_oneshot_mode();
#endif
	start_rt_timer(nano2count(TICK_PERIOD));
	rt_tasklet_use_fpu(&pt, 1);
	rt_insert_timer(&pt, 0, rt_get_time(), nano2count(TICK_PERIOD), ph, 0, 0);
	return 0;
}


void cleanup_module(void)
{
	stop_rt_timer();
	rt_shm_free(0xabcd);
}
