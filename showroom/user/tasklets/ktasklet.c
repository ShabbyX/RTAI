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

#include <asm/io.h>

#include <asm/rtai.h>
#include <rtai_sched.h>
#include <rtai_lxrt.h>
#include <rtai_tasklets.h>

#define TICK 1000000

static void rt_timer_tick(void)
{
	struct rt_tasklet_struct *tasklet;
	rt_times.tick_time = rt_times.intr_time;
	rt_times.intr_time = rt_times.tick_time + rt_times.periodic_tick;
	rt_set_timer_delay(0);
	if (rt_times.tick_time >= rt_times.linux_time) {
		if (rt_times.linux_tick > 0) {
			rt_times.linux_time += rt_times.linux_tick;
		}
		rt_pend_linux_irq(TIMER_8254_IRQ);
	}
	if ((tasklet = rt_find_tasklet_by_id(nam2num("TSKLET")))) {
		rt_exec_tasklet(tasklet);
	}
}

int init_module(void)
{
	rt_request_timer(rt_timer_tick, imuldiv(TICK, FREQ_8254, 1000000000), 0);
	return 0;
}


void cleanup_module(void)
{
	rt_free_timer();
}
