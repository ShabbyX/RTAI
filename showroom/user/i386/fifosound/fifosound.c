/*
COPYRIGHT (C) 2002-2008  Thomas Leibner (leibner@t-online.de)
                         Paolo Mantegazza (mantegazza@aero.polimi.it)

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
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/io.h>

#include <rtai_fifos.h>

#define PERIOD 125000

#define PORT_ADR 0x61

static volatile int end;

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
	RT_TASK *mytask;
	RTIME period;
	int playfifo, cntrfifo;
	char data, temp;

 	if (!(mytask = rt_thread_init(nam2num("SOUND"), 1, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT SOUND TASK\n");
		exit(1);
	}
	rtf_create(0, 2000);
	rtf_create(1, 1000);
	playfifo = 0;
	cntrfifo = 1;

	start_rt_timer(0);
	period = nano2count(PERIOD);
	printf("\nINIT SOUND TASK\n");
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();
	rt_task_make_periodic(mytask, rt_get_time() + 5*period, period);
	rtf_put(cntrfifo, &data, 1);

	while(!end) {
		if (rtf_get(playfifo, &data, 1) > 0) {
			data = filter(data);
			temp = inb(PORT_ADR);            
			temp &= 0xfd;
			temp |= (data & 1) << 1;
			outb(temp, PORT_ADR);
		}
		rt_task_wait_period();
		if (rtf_get(cntrfifo, &data, 1) > 0) {
			break;
		} 
	}

	rt_make_soft_real_time();
	stop_rt_timer();
	rtf_destroy(playfifo);
	rtf_destroy(cntrfifo);
	rt_task_delete(mytask);
	printf("\nEND SOUND TASK\n");
	return 0;
}

static void endme(int dummy) { end = 1; }

int main(void)
{
	int player;
	int playfifo, cntrfifo, thread;
	char data;

	signal(SIGINT,  endme);
	signal(SIGTERM, endme);
	signal(SIGHUP,  endme);
	signal(SIGALRM, endme);

	if ((player = open("../../../share/linux.au", O_RDONLY)) < 0) {
		printf("ERROR OPENING SOUND FILE (linux.au)\n");
		exit(1);
	}
	if ((playfifo = rtf_open_sized("/dev/rtf0", O_RDWR, 2000)) < 0) {
		printf("ERROR OPENING FIFO0\n");
		exit(1);
	}
	if ((cntrfifo = open("/dev/rtf1", O_RDWR)) < 0) {
		printf("ERROR OPENING FIFO1\n");
		exit(1);
	}

	thread = rt_thread_create(intr_handler, NULL, 10000);
	read(cntrfifo, &data, 1);
	printf("\nINIT MASTER TASK\n\n(CTRL-C TO END EVERYTHING)\n");

	while (!end) {	
		lseek(player, 0, SEEK_SET);
		while(!end && read(player, &data, 1) > 0) {
			write(playfifo, &data, 1);
		}
	}

	write(cntrfifo, &data, 1); 
	close(playfifo);
	close(cntrfifo);
	close(player);
	printf("\nEND MASTER TASK\n");
	rt_thread_join(thread);
	return 0;
}
