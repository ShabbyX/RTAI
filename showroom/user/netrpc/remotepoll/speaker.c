/*
COPYRIGHT (C) 2008  Paolo Mantegazza (mantegazza@aero.polimi.it)

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

int main(void)
{
	RT_TASK *spktsk, *plrtsk;
	RTIME period;
	MBX *mbx;
	char data, temp;
	unsigned long msg;
	struct rt_poll_s polld[1];

        if (!(spktsk = rt_task_init_schmod(nam2num("SPKTSK"), 1, 0, 0, SCHED_FIFO, 0x1))) {
                printf("CANNOT INIT SPEAKER TASK\n");
                exit(1);
        }
        mbx = rt_typed_named_mbx_init("SNDMBX", 1000, FIFO_Q);
	printf("\nSPEAKER TASK RUNNING %p\n", mbx);

	rt_set_oneshot_mode();
	start_rt_timer(0);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();
	printf("\nSPEAKER TASK RUNNING\n");

	period = nano2count(PERIOD);
	rt_task_make_periodic(spktsk, rt_get_time() + 5*period, period);

	while(1) {
#if 1
		polld[0] = (struct rt_poll_s){ mbx, RT_POLL_MBX_RECV };
		if (rt_poll(polld, 1, 0) != 1 || polld[0].what) {
			rt_printk("Speaker rt_poll failed\n");
		}
#endif
		if (!rt_mbx_receive_if(mbx, &data, 1)) {
			data = filter(data);
			temp = inb(PORT_ADR);            
			temp &= 0xfd;
			temp |= (data & 1) << 1;
			outb(temp, PORT_ADR);
		}
		rt_task_wait_period();
		if ((plrtsk = rt_receive_if(0, &msg))) {
			rt_return(plrtsk, msg);
			break;
		} 
	}

	rt_sleep(nano2count(100000000));
	rt_make_soft_real_time();
	rt_mbx_delete(mbx);
	stop_rt_timer();
	rt_task_delete(spktsk);
	printf("\nSPEAKER TASK STOPS\n");
	return 0;
}
