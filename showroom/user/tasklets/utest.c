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


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>

#include <rtai_lxrt.h>
#include <rtai_tasklets.h>

#define XCHECK

#define LOOPS 100

#define ONE_SHOT

#define TICK_PERIOD 10000000

#define PERIODIC_BUDDY 0

static volatile int end;

static struct rt_tasklet_struct *prt, *ost;

static int ostloops, prtloops;

static volatile long long firing_time, period;

void osh(unsigned long data)
{
	if (ostloops++ < LOOPS) {
#ifdef XCHECK
		if (ostloops == 1)
#endif
		rt_printk("\nFIRED BUDDY: %3d, %lx", ostloops, data);
#if PERIODIC_BUDDY == 0
		rt_insert_timer(ost, 0, firing_time += nano2count(TICK_PERIOD/2), 0, osh, 0xBBBBBBBB, 1);
#endif
	} else {
		rt_printk("\n");
		rt_remove_timer(ost);
	}
}

void prh1(unsigned long data)
{
	if (prtloops++ < LOOPS) {
#ifdef XCHECK
		if (prtloops == (LOOPS/2 + 1) || prtloops == (LOOPS - 500))
#endif
		rt_printk("\nFIRED NEW PERIODIC: %3d, %lx", prtloops, data);
	} else {
		rt_printk("\n");
		rt_remove_timer(prt);
		end = 1;
	}
}

void prh(unsigned long data)
{
	if (prtloops++ < LOOPS) {
#ifdef XCHECK
		if (prtloops == 1)
#endif
		rt_printk("\nFIRED PERIODIC: %3d, %lx", prtloops, data);
		if (prtloops == LOOPS/2) {
			rt_set_timer_period(prt, period *= 4);
			rt_set_timer_handler(prt, prh1);
			rt_set_timer_data(prt, 0xCCCCCCCC);
			rt_set_timer_data(ost, 0xDDDDDDDD);
		}
	}
}

int main(void)
{
	RT_TASK * main_rtai_task;
	struct rt_tasklets_struct *res;

	main_rtai_task = rt_task_init_schmod(rt_get_name(0), 10, 0, 0, SCHED_FIFO, 0xF);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	rt_make_hard_real_time();

#ifdef ONE_SHOT
	rt_set_oneshot_mode();
#endif
	firing_time = rt_get_time() + nano2count(100000000);
	period = nano2count(TICK_PERIOD);
	start_rt_timer(period);

	res = rt_create_timers(2);
	prt = rt_get_timer(res);
	rt_insert_timer(prt, 1, firing_time, period, prh, 0xAAAAAAAA, 1);
	ost = rt_get_timer(res);
	rt_insert_timer(ost, 0, firing_time, nano2count(PERIODIC_BUDDY*TICK_PERIOD/2), osh, 0xBBBBBBBB, 1);
	while(!end) sleep(1);
	stop_rt_timer();
	rt_gvb_timer(prt, res);
	rt_gvb_timer(ost, res);
	rt_destroy_timers(res);
	rt_task_delete(main_rtai_task);
	return 0;
}
