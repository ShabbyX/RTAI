/*
COPYRIGHT (C) 2009  Paolo Mantegazza (mantegazza@aero.polimi.it)

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
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>

#include <rtai_lxrt.h>

#define AVRGTIME    1
#define PERIOD      100000
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

static volatile int end;

static void async_callback(long syscall_nr, long retval)
{
	struct pollfd ufds = { 0, POLLIN, };
	printf("SYSCALL %ld, RETVAL %ld, ERRNO %d\n", syscall_nr, retval, retval < 0 ? errno : 0);
	if (retval < 0) {
		perror(NULL);
	}
	if (poll(&ufds, 1, 1)) {
		end = 1;
	}
}

int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in SPRT_ADDR;
	char buf[200];
	int fd;

	int diff;
	int sample;
	int average;
	int min_diff;
	int max_diff;
	int period;
	int i;
	int cnt;
	RTIME t, svt;
	RTIME expected, exectime[3];
	RT_TASK *task;
	long long max = -1000000000, min = 1000000000;
	struct sample { long long min; long long max; int index, ovrn; } samp;
	double s, sref;

 	if (!(task = rt_task_init_schmod(nam2num("LATCAL"), 0, 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT MASTER LATENCY TASK\n");
		exit(1);
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&SPRT_ADDR, sizeof(struct sockaddr_in));
	SPRT_ADDR.sin_family = AF_INET;
	SPRT_ADDR.sin_port = htons(5000);
	SPRT_ADDR.sin_addr.s_addr = inet_addr("127.0.0.1");

	fd = open("echo", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

	printf("\n## RTAI latency calibration tool ##\n");
	printf("# period = %i (ns) \n", PERIOD);
	printf("# average time = %i (s)\n", (int)AVRGTIME);
	printf("# use the FPU\n");
	printf("#%sstart the timer\n", argc == 1 ? " " : " do not ");
	printf("# timer_mode is %s\n", TIMER_MODE ? "periodic" : "oneshot");
	printf("\n");

	rt_sync_async_linux_syscall_server_create(NULL, ASYNC_LINUX_SYSCALL, async_callback, 120);

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
	s = 0.0;

	mlockall(MCL_CURRENT | MCL_FUTURE);

	rt_make_hard_real_time();
	expected = rt_get_time() + 200*period;
	rt_task_make_periodic(task, expected, period);
	svt = rt_get_cpu_time_ns();
	cnt = 0;
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
		if (max < samp.max) max = samp.max;
		if (min > samp.min) min = samp.min;
		++cnt;
//		printf("* %d - min: %lld/%lld, max: %lld/%lld average: %d <RET to stop> %d *\n", cnt, samp.min, min, samp.max, max, samp.index, samp.ovrn);
		sprintf(buf, "* %d - min: %lld/%lld, max: %lld/%lld average: %d <RET to stop> %d *\n", cnt, samp.min, min, samp.max, max, samp.index, samp.ovrn);
		i = write(STDOUT_FILENO, buf, strlen(buf));
		fdatasync(STDOUT_FILENO);
		i = write(fd, buf, strlen(buf));
		fsync(fd);
		sendto(sock, buf, strlen(buf), 0, (struct sockaddr *)&SPRT_ADDR, sizeof(struct sockaddr_in));
	}

	close(sock);
	close(fd);
	rt_make_soft_real_time();
	if (!hard_timer_running) {
		stop_rt_timer();
	}
	rt_get_exectime(task, exectime);
	if (exectime[1] && exectime[2]) {
		printf("\n>>> S = %g, EXECTIME = %G\n", s, (double)exectime[0]/(double)(exectime[2] - exectime[1]));
	}
	rt_task_delete(task);

	return 0;
}
