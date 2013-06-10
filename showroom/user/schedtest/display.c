/*
COPYRIGHT (C) 2007  Paolo Mantegazza (mantegazza@aero.polimi.it)

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
#include <sys/mman.h>

#include <asm/rtai_srq.h>
#include <rtai_lxrt.h>

#define LOOPS 500000000

#define DELAY 100000

#define PRINT_FREQ 1

#define MAKE_HARD()  rt_make_hard_real_time()

int main(void)
{
	int i, srq, count = 0, nextcount = 0, repeat;

	rt_thread_init(nam2num("MNTSK"), 100, 0, SCHED_FIFO, 0x1);
	rt_printk("\nTESTING THE SCHEDULER WITH SRQs [%d LOOPs].\n", LOOPS);
	repeat = 1000000000LL/((long long)DELAY*(long long)PRINT_FREQ);
	srq = rtai_open_srq(0xcacca);
	start_rt_timer(0);
	rt_grow_and_lock_stack(100000);
#ifdef MAKE_HARD
	MAKE_HARD();
#endif

	for (i = 0; i < LOOPS; i++) {
		rtai_srq(srq, (unsigned long)nano2count(DELAY));
		if (++count > nextcount) {
			nextcount += repeat;
			rt_printk(">>> %d.\n", nextcount);
		}
	}

	rt_make_soft_real_time();
	stop_rt_timer();
        rt_task_delete(NULL);
	rt_printk("END SCHEDULER TEST WITH SRQs.\n\n");
	return 0;
}
