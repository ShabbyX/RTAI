/*
 * Copyright (C) 2015 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <asm/io.h>

#include <asm/rtai.h>
#include <rtai_sched.h>
#include <rtai_fifos.h>
#include <rtai_nam2num.h>

MODULE_LICENSE("GPL");

#define PERIOD    100000
#define AVRGTIME  1
#define ECHO_FIFO 3
#define ECHOSPEED 1

#define MAXDIM 10
static double a[MAXDIM], b[MAXDIM];
static double dot(double *a, double *b, int n)
{
	int k; double s;
	for(k = n - 1, s = 0.0; k >= 0; k--) {
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

static int endme;

void fun(long dummy)
{
	int i, loops, average, period_counts, period = PERIOD, avrgtime = AVRGTIME;
	int diff = 0, min_diff = 0, max_diff = 0;
	RT_TASK *thread;
	RTIME expected;
	struct sample { long long min; long long max; int index, ovrn; } samp; 
	double dotres = 0.0;
	RTIME svt;

	rtf_put(ECHO_FIFO, &period, sizeof(period));
	rtf_put(ECHO_FIFO, &avrgtime, sizeof(avrgtime));

	thread = rt_thread_init(nam2num("KTHRLT"), 0, 1, SCHED_FIFO, 0xF);

	for(i = 0; i < MAXDIM; i++) {
		a[i] = b[i] = 3.141592;
	}

	loops = ((1000000000*avrgtime)/period)/ECHOSPEED;
	period_counts = nano2count(period);
	expected = rt_get_time() + 10*period_counts;
	rt_task_make_periodic(thread, expected, period_counts);

	svt = rt_get_cpu_time_ns();
	samp.ovrn = 0;
	while (!endme) {

		min_diff =  1000000000;
		max_diff = -1000000000;

		average = 0;

		for (i = 0; i < loops && !endme; i++) {
			expected += period_counts;
			if (!rt_task_wait_period()) {
				diff = count2nano(rt_get_time() - expected);
			} else {
				samp.ovrn++;
				diff = 0;
			}

			if (diff < min_diff) { min_diff = diff; }
			if (diff > max_diff) { max_diff = diff; }
			average += diff;
			dotres = dot(a, b, MAXDIM);
			do_some_io();
		}
		samp.min   = min_diff;
		samp.max   = max_diff;
		samp.index = average/loops;
		rtf_put(ECHO_FIFO, &samp, sizeof (samp));
	}
	rt_printk("\nDOT PRODUCT RESULT = %lu\n", (unsigned long)dotres);
	endme = 0;
}

static int __latency_init(void)
{
	rtf_create(ECHO_FIFO, 1000);
	start_rt_timer(0);
	rt_thread_create(fun, 0, 0);
	return 0;
}


static void __latency_exit(void)
{
	endme = 1;
	while(endme) msleep(10);
	stop_rt_timer();
	rtf_destroy(ECHO_FIFO);
}

module_init(__latency_init);
module_exit(__latency_exit);
