/*
 * Copyright (C) 2013 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#include <stdio.h>
#include <getopt.h>

#include <rtai_lxrt.h>

struct option options[] = {
	{ "help",       0, 0, 'h' },
	{ "period",     1, 0, 'p' },
	{ "time",       1, 0, 't' },
	{ NULL,         0, 0,  0  }
};

void print_usage(void)
{
    fputs(
	("\n*** KERNEL-USER SPACE LATENCY CALIBRATIONS ***\n"
	 "\n"
	 "OPTIONS:\n"
	 "  -h, --help\n"
	 "      print usage\n"
	 "  -p <period (us)>, --period <period (us)>\n"
	 "      the period of the hard real time calibrator tasks, default 200 (us)\n"
	 "  -t <duration (s)>, --time <duration (s)>\n"
	 "      the duration of the requested calibration, default 1 (s)\n"
	 "\n")
	, stderr);
}

static inline int sign(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }
static int period = 200 /* us */, loops = 1 /* s */, user_latency;
static RT_TASK *calmng;

int user_calibrator(long loops)
{
	RTIME expected;
	double s = 0;

 	if (!rt_thread_init(nam2num("USRCAL"), 0, 0, SCHED_FIFO, 0xF)) {
		printf("*** CANNOT INIT USER LATENCY CALIBRATOR TASK ***\n");
		return 1;
	}
	mlockall(MCL_CURRENT | MCL_FUTURE);

	rt_make_hard_real_time();
	expected = rt_get_time() + 10*period;
	rt_task_make_periodic(NULL, expected, period);
	while(loops--) {
		expected += period;
		rt_task_wait_period();
		user_latency += rt_get_time() - expected;
		s += 3.14;
	}
	rt_make_soft_real_time();
	rt_task_resume(calmng);

	rt_thread_delete(NULL);
	return s;
}

int main(int argc, char *argv[])
{
	int kern_latency;

	 while (1) {
		int c;
		if ((c = getopt_long(argc, argv, "hp:t:", options, NULL)) < 0) {
			break;
		}
		switch(c) {
			case 'h': { print_usage();         return 0; }
			case 'p': { period = atoi(optarg);    break; }
			case 't': { loops  = atoi(optarg);    break; }
		}
	}

 	if (!(calmng = rt_thread_init(nam2num("CALMNG"), 10, 0, SCHED_FIFO, 0xF)) ) {
		printf("*** CANNOT INIT CALIBRATION TASK ***\n");
		return 1;
	}
	printf("* CALIBRATING SCHEDULING LATENCIES FOR %d (s):", loops);
	loops  = (loops*1000000 + period/2)/period;
	printf(" PERIOD %d (us), LOOPS %d. *\n", period, loops);

	start_rt_timer(0);
	period = nano2count(1000*period);
	kern_latency = kernel_calibrator(period, loops);
	rt_thread_create((void *)user_calibrator, (void *)loops, 0);
	rt_task_suspend(calmng);
	stop_rt_timer();

	kern_latency = (kern_latency + loops/2)/loops;
	kern_latency = sign(kern_latency)*count2nano(abs(kern_latency));
	printf("* KERN SPACE LATENCY (or RETURN DELAY) %d (ns), ADD TO THE ONE CONFIGURED. *\n", kern_latency);
	user_latency = (user_latency + loops/2)/loops;
	user_latency = sign(user_latency)*count2nano(abs(user_latency));
	printf("* USER SPACE LATENCY (or RETURN DELAY) %d (ns), ADD TO THE ONE CONFIGURED. *\n", user_latency);
	rt_thread_delete(NULL);
	return 0;
}
