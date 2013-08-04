/*
 * COPYRIGHT (C) 2008  Paolo Mantegazza <mantegazza@aero.polimi.it>
 *
 * RTAI serial driver test
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>

#include <rtai_lxrt.h>
#include <rtai_serial.h>

#define WRITE_PORT  0
#define READ_PORT   1

#define MSGSIZE  1
struct msg_s { char msg[MSGSIZE]; RTIME time; int nr; };

static void write_fun(void *arg)
{
	int written;
	struct msg_s msg;

	msg.nr = 0;
	rt_task_init_schmod(nam2num("WRTSK"), 0, 0, 0, SCHED_FIFO, 0x1);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	if (rt_spopen(WRITE_PORT, 115200, 8, 1, RT_SP_PARITY_NONE, RT_SP_NO_HAND_SHAKE, RT_SP_FIFO_SIZE_4) < 0) {
		printf("serial test: rt_spopen for writer port error\n");
		goto exit_task;
	}

	while (1) {
		msg.time = rt_get_time_ns();
		msg.nr++;
		if ((written = rt_spwrite_timed(WRITE_PORT, (void *)&msg, sizeof(msg), DELAY_FOREVER))) {
			if (written < 0 ) {
				printf("rt_spwrite_timed, code %d\n", written);
			} else {
				printf("only %x instead of %d bytes transmitted\n", written, sizeof(msg));
			}
			goto exit_task;
		}
		rt_spread_timed(WRITE_PORT, (void *)&msg, sizeof(msg), DELAY_FOREVER);
		printf("   receiver check # %d, sent at: %lld (us)\n", msg.nr, msg.time/1000);
	}

exit_task:
	rt_spclose(WRITE_PORT);
	rt_make_soft_real_time();
	rt_task_delete(NULL);
	printf("write task exiting\n");
}

static void read_fun(void *arg)
{
	int read = 0, nr = 0, cnr = 0;
	struct msg_s msg;

	rt_task_init_schmod(nam2num("RDTSK"), 0, 0, 0, SCHED_FIFO, 0x1);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	if (rt_spopen(READ_PORT, 115200, 8, 1, RT_SP_PARITY_NONE, RT_SP_NO_HAND_SHAKE, RT_SP_FIFO_SIZE_4) < 0) {
		printf("serial test: rt_spopen for reader port error\n");
		goto exit_task;
	}

	while (1) {
		if (!(read = rt_spread_timed(READ_PORT, (void *)&msg, sizeof(msg), DELAY_FOREVER))) {
			printf("received as # %d, transm. time: %lld (us), sent as # %d\n", ++nr, (rt_get_time_ns() - msg.time)/1000, msg.nr);
		} else {
			if (read < 0) {
				printf("rt_spread_timed error, code %d\n", read);
			} else {
				printf("only %x instead of %d bytes received \n", read, sizeof(msg));
			}
			goto exit_task;
		}
		msg.nr = ++cnr;
		msg.time = rt_get_time_ns();
		rt_spwrite_timed(READ_PORT, (void *)&msg, sizeof(msg), DELAY_FOREVER);
	}

exit_task:
	rt_spclose(READ_PORT);
	rt_make_soft_real_time();
	rt_task_delete(NULL);
	printf("read task exiting (you can let the write task running if testing unconnected)\n");
}

static void catch_signal(int sig)
{
	rt_spclose(READ_PORT);
	rt_spclose(WRITE_PORT);
}

static pthread_t write_thread, read_thread;

int main(int argc, char* argv[])
{
	signal(SIGTERM, catch_signal);
	signal(SIGINT,  catch_signal);
	signal(SIGKILL, catch_signal);

	rt_task_init_schmod(nam2num("MAIN"), 1, 0, 0, SCHED_FIFO, 0xF);
	start_rt_timer(0);

	pthread_create(&read_thread,  NULL, (void *)read_fun, NULL);
	pthread_create(&write_thread, NULL, (void *)write_fun, NULL);

	pause();

	rt_task_delete(NULL);
	return 0;
}
