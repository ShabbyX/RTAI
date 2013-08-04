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
#include <sys/mman.h>

#include <rtai_tasklets.h>
#include <rtai_shm.h>
#include "ctest.h"

static volatile int end;

static struct rt_tasklet_struct *pt;

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
		end = 1;
	}
}

int main(void)
{
	struct sched_param mysched;

	mysched.sched_priority = sched_get_priority_max(SCHED_FIFO);
	if( sched_setscheduler( 0, SCHED_FIFO, &mysched ) == -1 ) {
		puts(" ERROR IN SETTING THE SCHEDULER UP");
		perror("errno");
		exit(0);
 	}
	mlockall(MCL_CURRENT | MCL_FUTURE);

	mem = rt_shm_alloc(0xabcd, 0, 0);
	pt = rt_init_timer();
	rt_insert_timer(pt, 1, rt_get_time(), nano2count(TICK_PERIOD), ph, 0, 1);
	while(!end) sleep(1);
	mem->flag = 1;
	rt_remove_timer(pt);
	rt_delete_timer(pt);
	rt_shm_free(0xabcd);
	return 0;
}
