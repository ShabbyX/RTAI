/*
COPYRIGHT (C) 2000  Paolo Mantegazza <mantegazza@aero.polimi.it>

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

//EXPORT_NO_SYMBOLS;

#define ONE_SHOT

#define TICK_PERIOD 10000000

#define PERIODIC_BUDDY 0

#define LOOPS 100

//#define FAST_SET

static struct rt_tasklet_struct prt, ost;

static int ostloops, prtloops;

static RTIME firing_time, period;

void osh(unsigned long data)
{
	if (ostloops++ < LOOPS) {
		rt_printk("\nFIRED BUDDY: %3d, %lx", ostloops, data);
#if PERIODIC_BUDDY == 0
		rt_insert_timer(&ost, 0, firing_time += nano2count(TICK_PERIOD/2), 0, osh, 0xBBBBBBBB, 0);
#endif
	} else {
		rt_printk("\n");
		rt_remove_timer(&ost);
	}
}

void prh1(unsigned long data)
{
	if (prtloops++ < LOOPS) {
		rt_printk("\nFIRED NEW PERIODIC: %3d, %lx", prtloops, data);
	} else {
		rt_printk("\n");
		rt_remove_timer(&prt);
	}
}

void prh(unsigned long data)
{
	if (prtloops++ < LOOPS) {
		rt_printk("\nFIRED PERIODIC: %3d, %lx", prtloops, data);
		if (prtloops == LOOPS/2) {
#ifdef FAST_SET
			rt_fast_set_timer_period(&prt, period *= 4);
			rt_fast_set_timer_handler(&prt, prh1);
			rt_fast_set_timer_data(&prt, 0xCCCCCCCC);
			rt_set_timer_data(&ost, 0xDDDDDDDD);
#else
			rt_set_timer_period(&prt, period *= 4);
			rt_set_timer_handler(&prt, prh1);
			rt_set_timer_data(&prt, 0xCCCCCCCC);
			rt_set_timer_data(&ost, 0xDDDDDDDD);
#endif
		}
	}
}

int init_module(void)
{
#ifdef ONE_SHOT
	rt_set_oneshot_mode();
#endif
	firing_time = rt_get_time() + nano2count(100000000);
	period = nano2count(TICK_PERIOD);
	start_rt_timer(period);
	rt_insert_timer(&prt, 1, firing_time, period, prh, 0xAAAAAAAA, 0);
	rt_insert_timer(&ost, 0, firing_time, nano2count(PERIODIC_BUDDY*TICK_PERIOD/2), osh, 0xBBBBBBBB, 0);
	return 0;
}

void cleanup_module(void)
{
	stop_rt_timer();
}
