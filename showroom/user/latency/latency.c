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
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/io.h>
#include <math.h>

#include <rtai_mbx.h>
#include <rtai_msg.h>

#define AVRGTIME    1
#define PERIOD      100000
#define TIMER_MODE  0
#define SOLO        0

#define SMPLSXAVRG ((1000000000*AVRGTIME)/PERIOD)/100

#define MAXDIM 10
static double a[MAXDIM], b[MAXDIM];

static double dot(double *a, double *b, int n)
{
        int k = n - 1;
        double s = 0.0;
        for(; k >= 0; k--) {
                s = s + a[k]*b[k];
        }
	return s;
}

#define MAXIO 3
static void do_some_io(void)
{
	int k = MAXIO;
	for(; k >= 0; k--) outb(1, 0x378);
}

static volatile int end;
void endme(int sig) { end = 1; }

static int hard_timer_running;

int main(int argc, char *argv[])
{
	int diff;
	int sample;
	long average;
	int min_diff;
	int max_diff;
	int period;
	int i;
	RTIME t, svt;
	RTIME expected, exectime[3];
	MBX *mbx;
	RT_TASK *task, *latchk;
	struct sample { long long min; long long max; int index, ovrn; } samp;
	double s = 0.0, sref;
	long long max = -1000000000, min = 1000000000;

	signal(SIGINT, endme);
	signal(SIGKILL, endme);
	signal(SIGTERM, endme);

 	if (!(mbx = rt_mbx_init(nam2num("LATMBX"), 20*sizeof(samp)))) {
		printf("CANNOT CREATE MAILBOX\n");
		exit(1);
	}

 	if (!(task = rt_task_init_schmod(nam2num("LATCAL"), 0, 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT MASTER LATENCY TASK\n");
		exit(1);
	}

	printf("\n## RTAI latency calibration tool ##\n");
	printf("# period = %i (ns) \n", PERIOD);
	printf("# average time = %i (s)\n", (int)AVRGTIME);
	printf("# use the FPU\n");
	printf("#%sstart the timer\n", argc == 1 ? " " : " do not ");
	printf("# timer_mode is %s\n", TIMER_MODE ? "periodic" : "oneshot");
	printf("\n");

	if (!(hard_timer_running = rt_is_hard_timer_running())) {
		if (TIMER_MODE) {
			rt_set_periodic_mode();
		} else {
			rt_set_oneshot_mode();
		}
		period = start_rt_timer(nano2count(PERIOD));
	} else {
		period = nano2count(PERIOD);
	}

        for(i = 0; i < MAXDIM; i++) {
                a[i] = b[i] = 3.141592;
        }
	sref = dot(a, b, MAXDIM);

	mlockall(MCL_CURRENT | MCL_FUTURE);

	rt_make_hard_real_time();
	expected = rt_get_time() + 200*period;
	rt_task_make_periodic(task, expected, period);
	svt = rt_get_cpu_time_ns();
	i = 0;
	samp.ovrn = 0;
	while (!end) {
		min_diff = 1000000000;
		max_diff = -1000000000;
		average = 0;

		for (sample = 0; sample < SMPLSXAVRG && !end; sample++) {
			expected += period;
			if (!rt_task_wait_period()) {
				if (TIMER_MODE) {
					diff = (int) ((t = rt_get_cpu_time_ns()) - svt - PERIOD);
					svt = t;
				} else {
					diff = (int) count2nano(rt_get_time() - expected);
				}
			} else {
				samp.ovrn++;
				diff = 0;
				if (TIMER_MODE) {
					svt = rt_get_cpu_time_ns();
				}
			}
			outb(i = 1 - i, 0x378);

			if (diff < min_diff) {
				min_diff = diff;
			}
			if (diff > max_diff) {
				max_diff = diff;
			}
			average += diff;
			s = dot(a, b, MAXDIM);
			do_some_io();
			if (fabs((s - sref)/sref) > 1.0e-15) {
				printf("\nDOT PRODUCT RESULT = %20.16e %20.16e %20.16e\n", s, sref, fabs((s - sref)/sref));
				return 0;
			}
		}
		samp.min = min_diff;
		samp.max = max_diff;
		samp.index = average/SMPLSXAVRG;
#if SOLO
		if (max < samp.max) max = samp.max;
		if (min > samp.min) min = samp.min;
		rt_printk("SOLO * min: %lld/%lld, max: %lld/%lld average: %d (%d) <Hit [RETURN] to stop> *\n", samp.min, min, samp.max, max, samp.index, samp.ovrn);
#else
		rt_mbx_send_if(mbx, &samp, sizeof(samp));
		if ((latchk = rt_get_adr(nam2num("LATCHK"))) && (rt_receive_if(latchk, &average) || end)) {
			rt_return(latchk, average);
			break;
		}
#endif
	}

	while (rt_get_adr(nam2num("LATCHK"))) {
		rt_sleep(nano2count(1000000));
	}
	rt_make_soft_real_time();
	if (!hard_timer_running) {
		stop_rt_timer();	
	}
	rt_get_exectime(task, exectime);
	if (exectime[1] && exectime[2]) {
		printf("\n>>> S = %g, EXECTIME = %G\n", s, (double)exectime[0]/(double)(exectime[2] - exectime[1]));
	}
	rt_task_delete(task);
	rt_mbx_delete(mbx);

	return 0;
}
