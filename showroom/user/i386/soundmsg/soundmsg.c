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
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/io.h>

#include <rtai_msg.h>

#define PERIOD 125000

#define PORT_ADR 0x61

#define BUFSIZE 200

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

static volatile int end;

static void *speaker_handler(void *args)
{
	RT_TASK *mytask, *master;
	RTIME period;
	char data, buf[BUFSIZE];
	char temp;
	long i, len;

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	ioperm(PORT_ADR, 1, 1);

 	if (!(mytask = rt_task_init_schmod(nam2num("SOUND"), 1, 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT SOUND TASK\n");
		exit(1);
	}
	mlockall(MCL_CURRENT | MCL_FUTURE);

	printf("\nINIT SOUND TASK %p\n", mytask);

	rt_make_hard_real_time();
	period = nano2count(PERIOD);
	rt_sendx(rt_get_adr(nam2num("SNDMAS")), buf, 1);
	rt_task_make_periodic(mytask, rt_get_time() + 100*period, period);

	len = 0;
	while(!end) {
		if (len) {
			data = filter(buf[i++]);
			temp = inb(PORT_ADR);
			temp &= 0xfd;
			temp |= (data & 1) << 1;
			outb(temp, PORT_ADR);
			len--;
		} else {
			if (rt_evdrpx(0, buf, BUFSIZE, &i)) {
//				rt_printk("EVDRP %d\n", i);
			}
			if ((master = rt_receivex_if(0, buf, BUFSIZE, &len))) {
				rt_returnx(master, &len, sizeof(int));
				if (len == sizeof(int) && ((int *)buf)[0] == 0xFFFFFFFF) {
					break;
				}
				i = 0;
			}
		}
		rt_task_wait_period();
	}

	rt_make_soft_real_time();
	rt_task_delete(mytask);
	printf("\nEND SOUND TASK %p\n", mytask);
	return 0;
}

static pthread_t thread;

static void endme(int dummy) { end = 1; }

int main(void)
{
	long i, player, msg;
	RT_TASK *mytask;
	char data[BUFSIZE];

	signal(SIGINT, endme);
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

	rt_set_oneshot_mode();
	start_rt_timer(0);
	pthread_create(&thread, NULL, speaker_handler, NULL);
	rt_receivex(0, data, 1, &i);

	while (!end) {
		lseek(player, 0, SEEK_SET);
		while(!end && (i = read(player, data, BUFSIZE)) > 0) {
			rt_rpcx(rt_get_adr(nam2num("SOUND")), data, &msg, i, sizeof(int));
			if (msg != i) {
				printf("SPEAKER RECEIVED LESS THAN SENT BY PLAYER\n");
			}
		}
	}

	msg = 0xFFFFFFFF;
	rt_rpcx(rt_get_adr(nam2num("SOUND")), &msg, &msg, sizeof(int), 1);
	pthread_join(thread, NULL);
	rt_task_delete(mytask);
	stop_rt_timer();
	close(player);
	printf("\nEND MASTER TASK %p\n", mytask);
	return 0;
}
