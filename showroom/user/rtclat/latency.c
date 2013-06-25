/*
COPYRIGHT (C) 2005  Paolo Mantegazza (mantegazza@aero.polimi.it)

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

#include <rtai_mbx.h>
#include <rtai_msg.h>

#if CONFIG_RTAI_RTC_FREQ < 2
#undef CONFIG_RTAI_RTC_FREQ
#define CONFIG_RTAI_RTC_FREQ 10000
#endif

#define AVRGTIME    1
#define PERIOD      (1000000000/CONFIG_RTAI_RTC_FREQ)
#define SMPLSXAVRG  ((1000000000*AVRGTIME)/PERIOD)/1000

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

int main(int argc, char *argv[])
{
	int diff;
	int sample;
	int average;
	int min_diff;
	int max_diff;
	int period;
	int i;
	RTIME t, svt, exectime[3];
	MBX *mbx;
	RT_TASK *task;
	struct sample { long long min; long long max; int index, ovrn; } samp;
	double s;

 	if (!(mbx = rt_mbx_init(nam2num("LATMBX"), 20*sizeof(samp)))) {
		printf("CANNOT CREATE MAILBOX\n");
		exit(1);
	}

 	if (!(task = rt_task_init_schmod(nam2num("LATCAL"), 0, 0, 0, SCHED_FIFO, 0x1))) {
		printf("CANNOT INIT MASTER LATENCY TASK\n");
		exit(1);
	}

	printf("\n## RTAI latency calibration tool ##\n");
	printf("# period = %i (ns) \n", PERIOD);
	printf("# average time = %i (s)\n", (int)AVRGTIME);
	printf("# use the FPU\n");
	printf("#%sstart the timer\n", argc == 1 ? " " : " do not ");
	printf("# timer_mode is %s\n", "RTC periodic");
	printf("\n");

	rt_set_periodic_mode();
	period = start_rt_timer(nano2count(PERIOD));

	for(i = 0; i < MAXDIM; i++) {
		a[i] = b[i] = 3.141592;
	}
	s = dot(a, b, MAXDIM);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();
	rt_task_make_periodic(task, rt_get_time() + 10*period, period);
	i = 0;
	samp.ovrn = 0;

	svt = rt_get_cpu_time_ns();
	while (1) {
		min_diff = 1000000000;
		max_diff = -1000000000;
		average = 0;

		for (sample = 0; sample < SMPLSXAVRG; sample++) {
			if (!rt_task_wait_period()) {
				diff = (int) ((t = rt_get_cpu_time_ns()) - svt - PERIOD);
				svt = t;
			} else {
				samp.ovrn++;
				diff = 0;
				svt = rt_get_cpu_time_ns();
			}
			if (!sample) continue;
			outb(i = 1 - i, 0x378);

			if (diff < min_diff) {
				min_diff = diff;
			}
			if (diff > max_diff) {
				max_diff = diff;
			}
			average += diff;
			s = dot(a, b, MAXDIM);
		}
		samp.min = min_diff;
		samp.max = max_diff;
		samp.index = average/SMPLSXAVRG;
		rt_mbx_send_if(mbx, &samp, sizeof(samp));
		if (rt_receive_if(rt_get_adr(nam2num("LATCHK")), (unsigned int *)&average)) {
			rt_return(rt_get_adr(nam2num("LATCHK")), (unsigned int)average);
			break;
		}
	}

	while (rt_get_adr(nam2num("LATCHK"))) {
		rt_sleep(nano2count(1000000));
	}
	rt_make_soft_real_time();
	stop_rt_timer();
	rt_get_exectime(task, exectime);
	printf("\n>>> S = %g, EXECTIME = %G\n", s, (double)exectime[0]/(double)(exectime[2] - exectime[1]));
	rt_task_delete(task);
	rt_mbx_delete(mbx);

	return 0;
}
