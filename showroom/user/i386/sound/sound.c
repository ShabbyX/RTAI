/*
COPYRIGHT (C) 1999  Paolo Mantegazza <mantegazza@aero.polimi.it>

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
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/io.h>
#include <math.h>

#include <rtai_mbx.h>
#include <rtai_msg.h>

#define PERIOD 125000

#define PORT_ADR 0x61

#define BUFSIZE 1

static int filter(int x)
{
	static int oldx;
	int ret;

	if (x & 0x80) {
		x = 382 - x;
	}
	ret = x > oldx;
	oldx = x;
	return ret;
}

static void *intr_handler(void *args)
{
	RT_TASK *mytask, *master;
	RTIME period;
	MBX *mbx;
	char data = 'G';
	char temp;
	unsigned long msg;

	rt_allow_nonroot_hrt();
//	ioperm(PORT_ADR, 1, 1);
	iopl(3);

 	if (!(mytask = rt_task_init_schmod(nam2num("SOUND"), 1, 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT SOUND TASK\n");
		exit(1);
	}
	mbx = rt_typed_named_mbx_init("SNDMBX", 2000, FIFO_Q);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	printf("\nINIT SOUND TASK %p\n", mytask);

	rt_make_hard_real_time();
	period = nano2count(PERIOD);
	rt_mbx_send(mbx, &data, 1);
	rt_task_make_periodic(mytask, rt_get_time() + 100*period, period);

	while(1) {
		if (!rt_mbx_receive_if(mbx, &data, 1)) {
			data = filter(data);
			temp = inb(PORT_ADR);
			temp &= 0xfd;
			temp |= (data & 1) << 1;
			outb(temp, PORT_ADR);
		}
		rt_task_wait_period();
		if ((master = rt_receive_if(0, &msg))) {
			rt_return(master, msg);
			break;
		}
	}

	rt_make_soft_real_time();
	rt_task_delete(mytask);
	printf("\nEND SOUND TASK %p\n", mytask);
	return 0;
}

static int end;

static void endme(int dummy) { end = 1; }

int main(void)
{
	pthread_t thread;
	unsigned int player, cnt;
	unsigned long msg;
	RT_TASK *mytask;
	MBX *mbx;
	char data[BUFSIZE];

	signal(SIGINT, endme);
	rt_allow_nonroot_hrt();

	if ((player = open("../../../share/linux.au", O_RDONLY)) < 0) {
		printf("ERROR OPENING SOUND FILE (linux.au)\n");
		exit(1);
	}
 	if (!(mytask = rt_task_init(nam2num("SNDMAS"), 2, 0, 0))) {
		printf("CANNOT INIT MASTER TASK\n");
		exit(1);
	}
	mlockall(MCL_CURRENT | MCL_FUTURE);
	printf("\nINIT MASTER TASK %p\n\n(CTRL-C TO END EVERYTHING)\n", mytask);

	mbx = rt_typed_named_mbx_init("SNDMBX", 2000, FIFO_Q);
	rt_set_oneshot_mode();
	start_rt_timer(0);
	thread = rt_thread_create(intr_handler, NULL, 10000);
	rt_mbx_receive(mbx, &data, 1);

	while (!end) {
		lseek(player, 0, SEEK_SET);
		while(!end && (cnt = read(player, &data, BUFSIZE)) > 0) {
			rt_mbx_send(mbx, data, cnt);
		}
	}

	rt_rpc(rt_get_adr(nam2num("SOUND")), msg, &msg);
	while (rt_get_adr(nam2num("SOUND"))) {
		rt_sleep(nano2count(1000000));
	}
	rt_task_delete(mytask);
	rt_mbx_delete(mbx);
	stop_rt_timer();
	close(player);
	printf("\nEND MASTER TASK %p\n", mytask);
	rt_thread_join(thread);
	return 0;
}
