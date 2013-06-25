/*
COPYRIGHT (C) 2002  Paolo Mantegazza (mantegazza@aero.polimi.it)

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

#define SSE

#ifdef THREEDNOW
#define vec_mul_mr_a vec_mul_mr
#define SIMD_T float
#else
#define vec_enter()
#define vec_exit()
#ifdef SSE
#define SIMD_T float
#else
#define SIMD_T double
#endif
#endif

#include <SSE3Dnow.h>

#include <rtai_lxrt.h>
#include <rtai_mbx.h>
#include <rtai_msg.h>

#define OVERALL

#define SKIP 4000

#define PERIOD 100000

#define MAXDIM 1024  // must be multiple of 4

static SIMD_T a[MAXDIM], b[MAXDIM] __attribute__((aligned(16)));

#if 1
static SIMD_T vdot(SIMD_T a[], SIMD_T b[], int n)
{
	int i;
	vector s = { 0.0, 0.0, 0.0, 0.0 };
	vec_enter();
	vec_mov_mr(s, reg0);
	for (i = n - VECLEN; i >= 0; i -= VECLEN) {
		vec_mov_mr_a(a + i, reg1);
		vec_mul_mr_a(b + i, reg1);
		vec_add_rr(reg1, reg0);
	}
	vec_mov_rm(reg0, s);
	vec_exit();
#if VECLEN == 2
	return s[0] + s[1];
#else
	return s[0] + s[1] + s[2] + s[3];
#endif
}
#else
static SIMD_T vdot(SIMD_T a[], SIMD_T b[], int n)
{
	int k = n - 1;
	double s = 0.0;
	for(; k >= 0; k--) {
		s = s + a[k]*b[k];
	}
	return s;
}
#endif

int main(int argc, char *argv[])
{
	int diff;
	int skip;
	int average;
	int min_diff;
	int max_diff;
	int period;
	int i;
	RTIME expected, ts;
	MBX *mbx;
	RT_TASK *task;
	struct sample { long long min; long long max; int index; double s; int ts; } samp;
	double s;

 	if (!(mbx = rt_mbx_init(nam2num("LATMBX"), 20*sizeof(samp)))) {
		printf("CANNOT CREATE MAILBOX\n");
		exit(1);
	}

 	if (!(task = rt_task_init_schmod(nam2num("LATCAL"), 0, 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT MASTER TASK\n");
		exit(1);
	}

	if (argc == 1) {
		rt_set_oneshot_mode();
		start_rt_timer(0);
	}

	for(i = 0; i < MAXDIM; i++) {
		a[i] = b[i] = 3.141592;
	}
	vdot(a, b, MAXDIM);

	mlockall(MCL_CURRENT | MCL_FUTURE);

	rt_make_hard_real_time();
	period = nano2count(PERIOD);
	rt_task_make_periodic(task, expected = rt_get_time() + 5*period, period);

#ifdef OVERALL
	min_diff = 1000000000;
	max_diff = -1000000000;
#endif
	while (1) {
#ifndef OVERALL
		min_diff = 1000000000;
		max_diff = -1000000000;
#endif
		average = 0;

		for (skip = 0; skip < SKIP; skip++) {
			expected += period;
			rt_task_wait_period();

			diff = (int)count2nano(rt_get_time() - expected);
			if (diff < min_diff) {
				min_diff = diff;
			}
			if (diff > max_diff) {
				max_diff = diff;
			}
			average += diff;
			ts = rt_get_time();
			s = vdot(a, b, MAXDIM);
			ts = rt_get_time() - expected;
		}
		samp.min = min_diff;
		samp.max = max_diff;
		samp.index = average/SKIP;
		samp.s = (double)s;
		samp.ts = (int)count2nano(ts);
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
	if (argc == 1) {
		stop_rt_timer();
	}
	rt_task_delete(task);
	rt_mbx_delete(mbx);

	return 0;
}
