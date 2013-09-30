/*
 *  * (C) Pierre Cloutier <pcloutier@PoseidonControls.com> LGPLv2
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <ctype.h>
#include <rtai_msg.h>

#define PRINTF rt_printk

static char msg[512], rep[512];

int main(int argc, char* argv[])
{
	unsigned long srv_name = nam2num("SRV");
	RT_TASK *srv;
	pid_t pid, my_pid, proxy;
	int count;
	size_t msglen;
	RTIME period;
	char *pt;

 	if (!(srv = rt_task_init_schmod(srv_name, 0, 0, 0, SCHED_FIFO, 0x1))) {
		PRINTF("CANNOT INIT SRV TASK\n");
		exit(-1);
	}
	mlockall(MCL_CURRENT | MCL_FUTURE);

	my_pid = rt_Alias_attach("");
	if (my_pid <= 0) {
		PRINTF("Cannot attach name SRV\n");
		exit(-1);
	}

//	rt_set_oneshot_mode();
	period = nano2count(1000000);
	start_rt_timer(period);
	rt_make_hard_real_time();
	PRINTF("SRV starts (task = %p, pid = %d)\n", srv, my_pid);

	rt_task_make_periodic(srv, rt_get_time(), period);

  	proxy = rt_Proxy_attach(0, "More beer please", 17, -1);
	if (proxy <= 0 ) {
		PRINTF("Failed to attach proxy\n");
		exit(-1);
	}

	msglen = 0;
	pid = rt_Receive(0, 0, 0, &msglen);
	if (pid) {
		// handshake to give the proxy to CLT
		PRINTF("rt_Reply the proxy %04X msglen = %d\n", proxy, msglen);
		rt_Reply(pid, &proxy, sizeof(proxy));
	}

	rt_sleep(nano2count(1000000000));
	count = 20;
	while(count--) {
		memset( msg, 0, sizeof(msg));
		pid = rt_Receive(0, msg, sizeof(msg), &msglen);
		if(pid == proxy) {
			PRINTF("SRV receives the PROXY event [%s]\n", msg);
			continue;
		} else if (pid <= 0) {
			PRINTF("SRV rt_Receive() failed\n");
			continue;
		}

		PRINTF("SRV received msg    [%s] %d bytes from pid %04X\n", msg, msglen, pid);

		memcpy (rep, msg, sizeof(rep));
		pt = (char *) rep;
		while (*pt) {
			*pt = toupper(*pt);
			pt++;
		}
		if (rt_Reply(pid, rep, sizeof(rep))) {
			PRINTF("SRV rt_Reply() failed\n");
		}
	}

	if (rt_Proxy_detach(proxy)) {
		PRINTF("SRV cannot detach proxy\n");
	}
	if (rt_Name_detach(my_pid)) {
		PRINTF("SRV cannot detach name\n");
	}
	rt_make_soft_real_time();
	rt_sleep(nano2count(1000000000));
	if (rt_task_delete(srv)) {
		PRINTF("SRV cannot delete task\n");
	}
	return 0;
}
