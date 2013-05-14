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
#include <string.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/io.h>

#include <rtai_netrpc.h>

#define PERIOD 125000

#define BUFSIZE 3000

#define PORT_ADR 0x61

static inline int filter(int x)
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

int main(int argc, char *argv[])
{
	unsigned int plrnode, plrport;
	RT_TASK *spktsk, *plrtsk;
	RTIME period;
	struct sockaddr_in addr;
	char buf[BUFSIZE], data, temp;
	unsigned int msg;
	long i, len;

	printf("\n\nSPECIFIC RECEIVE DIRECTLY ON REMOTE TASK\n");
	ioperm(PORT_ADR, 1, 1);
        if (!(spktsk = rt_task_init_schmod(nam2num("SPKTSK"), 1, 0, 0, SCHED_FIFO, 0xF))) {
                printf("CANNOT INIT SPEAKER TASK\n");
                exit(1);
        }

        rt_returnx(rt_receivex(0, &msg, sizeof(int), &len), &msg, 1);
        plrnode = 0;
        if (argc == 2 && strstr(argv[1], "PlrNode=")) {
                inet_aton(argv[1] + 8, &addr.sin_addr);
                plrnode = addr.sin_addr.s_addr;
        }
        if (!plrnode) {
                inet_aton("127.0.0.1", &addr.sin_addr);
                plrnode = addr.sin_addr.s_addr;
        }

	rt_set_oneshot_mode();
	start_rt_timer(0);
	while ((plrport = rt_request_port(plrnode)) <= 0 && plrport != -EINVAL);
	while (!(plrtsk = RT_get_adr(plrnode, plrport, "PLRTSK"))) {
		rt_sleep(nano2count(10000000));
	}

	printf("\nSPEAKER TASK RUNNING\n");

	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	period = nano2count(PERIOD);
	rt_task_make_periodic(spktsk, rt_get_time() + 5*period, period);

        for (i = 0; i < 100; i++) {
		RT_returnx(plrnode, plrport, RT_receivex(plrnode, plrport, plrtsk, &msg, sizeof(int), &len), &msg, sizeof(int));
        }

	len = 0;
	while(1) {
		if (len) {
			data = filter(buf[i++]);
			temp = inb(PORT_ADR);
			temp &= 0xfd;
			temp |= (data & 1) << 1;
			outb(temp, PORT_ADR);
			len--;
		} else {
			if (RT_evdrpx(plrnode, plrport, plrtsk, buf, BUFSIZE, &i)) {
//				rt_printk("EVDRP %d\n", i);
			}
			if (RT_receivex_if(plrnode, plrport, plrtsk, buf, BUFSIZE, &len) == plrtsk) {
				RT_returnx(plrnode, plrport, plrtsk, &len, sizeof(int));
				if (len == sizeof(int) && ((int *)buf)[0] == 0xFFFFFFFF) {
					break;
				}
				i = 0;
			}
		}
		rt_task_wait_period();
	}

	rt_sleep(nano2count(100000000));
	rt_make_soft_real_time();
	stop_rt_timer();
	rt_release_port(plrnode, plrport);
	rt_task_delete(spktsk);
	printf("\nSPEAKER TASK STOPS\n");
	return 0;
}
