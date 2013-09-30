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
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
#include <asm/io.h>
#include <math.h>

#include <rtai_mbx.h>
#include <rtai_msg.h>

#define AVRGTIME    1
#define PERIOD      200000
#define TIMER_MODE  0

#define SMPLSXAVRG ((1000000000*AVRGTIME)/PERIOD)/10

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

static int hard_timer_running;

int main(int argc, char *argv[])
{
	int diff;
	int sample;
	int average;
	int min_diff;
	int max_diff;
	int period;
	int i;
	RTIME t, svt;
	RTIME expected;
	MBX *mbx;
	RT_TASK *task;
	struct sample { RT_TASK *task; int cnt; long long min; long long max; int index, ovrn; } samp;
	double s, sref;

	if (!(mbx = rt_get_adr(nam2num("LATMBX")))) {
	 	if (!(mbx = rt_mbx_init(nam2num("LATMBX"), 40*sizeof(samp)))) {
			printf("CANNOT CREATE MAILBOX\n");
			exit(1);
		}
	}

	samp.cnt = 0;
 	if (!(samp.task = task = rt_task_init_schmod(rt_get_name(NULL), 0, 0, 0, SCHED_FIFO, 0xF))) {
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
	expected = rt_get_time() + 2*period;
	rt_task_make_periodic(task, expected, period);
	svt = rt_get_cpu_time_ns();
	i = 0;
	samp.ovrn = 0;
	while (1) {
		min_diff = 1000000000;
		max_diff = -1000000000;
		average = 0;

		for (sample = 0; sample < SMPLSXAVRG; sample++) {
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
			if (fabs(s/sref - 1.0) > 1.0e-16) {
				printf("\nDOT PRODUCT RESULT = %lf %lf %lf\n", s, sref, sref - s);
				return 0;
			}
		}
		samp.min = min_diff;
		samp.max = max_diff;
		samp.index = average/SMPLSXAVRG;
		samp.cnt++;
		rt_mbx_send_if(mbx, &samp, sizeof(samp));
	}

	rt_make_soft_real_time();
	rt_mbx_delete(mbx);

	return 0;
}
