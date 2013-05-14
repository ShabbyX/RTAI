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


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>

#include <rtai_tasklets.h>

#define ONE_SHOT

#define TICK_PERIOD 1000000

#define LOOPS 1000

static volatile int end, endp;

static struct rt_tasklet_struct *prt, *seqt;

static int prtloops, opcod, poloops;

static volatile long long firing_time, period;

void seqh(unsigned long data)
{
	switch (opcod) {
		case 0: {
			rt_printk("- SEQUENCER FIRED FOR A FAST POLLING\n");
			rt_insert_timer(seqt, 10, rt_get_time() + nano2count(500000), nano2count(500000), seqh, 0, 1);
			opcod = 1;
		}
		break;
		case 1: {
			if (poloops++ < LOOPS) {
				if (poloops == 1) {
					rt_printk("- SEQUENCER BEGINS FAST POLLING\n");
				}
			} else {	
				rt_printk("- SEQUENCER FAST POLLING ENDED, TIMED FOR A LATER FINAL OPERATION\n");
				rt_set_timer_firing_time(seqt, rt_get_time() + nano2count(50000000));
				opcod = 2;
			}
		}
		break;
		case 2: {
			rt_printk("- SEQUENCER EXECUTES FINAL OPERATION, ... THEN\n  SLOWLY POLLS WAITING FOR THE END OF THE PERIODIC COMPUTATIONAL TASKLET\n");
				opcod = 3;
		}
		break;
		case 3: {
			if (endp) {
				rt_printk("- PERIODIC COMPUTATIONAL TASKLET GONE, SEQUENCER ENDS IT ALL\n");
				rt_remove_timer(seqt);
				end = 1;
			} else {
//				rt_printk("\nSEQUENCER WAITING FOR PERIODIC TIMER END");
			}
		}
		break;
	}
}

void prh(unsigned long data)
{
	int i;
	double a[100], s;

	if (prtloops++ < LOOPS) {
		if (prtloops == 1) {
			rt_printk("+ PERIODIC COMPUTATIONAL TASKLET FIRED\n");
		}
		for (i = 0; i < 100; i++) {
			a[i] = i + 1;
		}	
		s = 0;
		for (i = 1; i < 100; i++) {
			s += (a[i - 1]*a[i]);
		}	
	} else {
		rt_printk("+ PERIODIC COMPUTATIONAL TASKLET ENDED\n");
		rt_remove_timer(prt);
		endp = 1;
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
	rt_grow_and_lock_stack(20000);

#ifdef ONE_SHOT
	rt_set_oneshot_mode();
#endif
	start_rt_timer(nano2count(TICK_PERIOD));
	firing_time = rt_get_time();
	period = nano2count(1000000);
	prt = rt_init_timer();
	rt_insert_timer(prt, 1, firing_time, period, prh, 0, 1);
	rt_tasklet_use_fpu(prt, 1);
	seqt = rt_init_timer();
	rt_insert_timer(seqt, 0, firing_time, 0, seqh, 0, 1);
	while(!end) sleep(1);
	stop_rt_timer();
	rt_delete_timer(prt);
	rt_delete_timer(seqt);
	return 0;
}
