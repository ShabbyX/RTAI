/*
COPYRIGHT (C) 2008  Paolo Mantegazza (mantegazza@aero.polimi.it)

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


#include <rtai_lxrt.h>
#include <rtai_shm.h>
#include <rtai_msg.h>
#include <rtai_usi.h>

#define PERIOD        25000
#define WAIT_DELAY    18000
#define WORKING_TIME  5000

#define WAIT_AIM_TIME()  do { while (rt_get_time() < aim_time); } while (0)

#define MAXDIM 1000
static double dot(double *a, double *b, int n)
{
	int k = n - 1;
	double s = 0.0;
	for(; k >= 0; k--) {
		s = s + a[k]*b[k];
	}
	return s;
}

int main (int argc, char **argv)
{
	volatile double s, a[MAXDIM], b[MAXDIM];
	long *worst_lat, msg;
	RTIME period, wait_delay, sync_time, aim_time;

	for(msg = 0; msg < MAXDIM; msg++) {
		a[msg] = b[msg] = 3.141592;
	}
	rt_allow_nonroot_hrt();
	start_rt_timer(0);
	rt_grow_and_lock_stack(100000);
	worst_lat = rt_shm_alloc(nam2num("WSTLAT"), sizeof(RTIME), USE_VMALLOC);
	*worst_lat = -2000000000;
	rt_task_init_schmod(nam2num("LOOPER"), 0, 0, 0, SCHED_FIFO, 0x2);
	wait_delay = nano2count(WAIT_DELAY);
	period     = nano2count(PERIOD);

	rt_make_hard_real_time();
	rtai_cli();
	aim_time  = rt_get_time();
	sync_time = aim_time + wait_delay;
	aim_time += period;
	while (!rt_receive_if(NULL, &msg)) {
		WAIT_AIM_TIME();
		sync_time = rt_get_time();
		msg = abs((long)(sync_time - aim_time));
		sync_time = aim_time + wait_delay;
		aim_time  += period;
		if (msg > *worst_lat) {
			*worst_lat = msg;
		}
		s = dot(a,b, MAXDIM);
		rt_busy_sleep(WORKING_TIME);
		rt_sleep_until(sync_time);
	}
	rtai_sti();
	rt_make_soft_real_time();

	stop_rt_timer();
	rt_shm_free(nam2num("WSTLAT"));
	rt_task_delete(NULL);
	return 0;
}
