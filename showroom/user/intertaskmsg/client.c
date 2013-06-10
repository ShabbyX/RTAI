#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sched.h>
#include <rtai_msg.h>

#define PRINTF rt_printk

int main(int argc, char* argv[])
{
	unsigned long clt_name = nam2num("CLT");
	int count, len, err;
	pid_t pid, my_pid, proxy;
	RT_TASK *clt;
	char msg[512], rep[512];

	// Give a lower priority than SRV and proxy.
 	if (!(clt = rt_task_init_schmod(clt_name, 1, 0, 0, SCHED_FIFO, 0x1))) {
		PRINTF("CANNOT INIT CLIENT TASK\n");
		exit(3);
	}
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	if ((my_pid = rt_Alias_attach("CLIENT")) <= 0) {
		PRINTF("Cannot attach name CLT\n");
		exit(4);
	}

	rt_sleep(nano2count(1000000000));

	PRINTF("CLT starts (task = %p, pid = %d)\n", clt, my_pid);

 	if ((pid = rt_Name_locate("", "SRV")) <= 0) {
		PRINTF("Cannot locate SRV\n");
		exit(5);
	}

	len = rt_Send(pid, 0, &proxy, 0, sizeof(proxy));

	if (len == sizeof(proxy)) {
		PRINTF("CLT got the proxy %04X\n", proxy);
		count = 10;
		while (count--) {
			err = rt_Trigger(proxy);
			if (err!=pid) {
				PRINTF("Failed to send the proxy\n");
			}
		}
	} else {
		PRINTF("Failed to receive the proxy pid\n");
	}

	count = 10;
	while (count--) {
  		PRINTF("CLT sends to SRV\n" );
		strcpy(msg, "Hello Beautifull Linux World");
		memset(rep, 0, sizeof(rep));
		len = rt_Send( pid, msg, rep, sizeof(msg), sizeof(rep));
		if (len < 0) {
			PRINTF("CLT: rt_Send() failed\n");
			break;
		}
		PRINTF("CLT: reply from SRV [%s] %d\n", rep, len);
		if (count) {
			rt_sleep(nano2count(1000000000));
		}
	}

	if (rt_Name_detach(my_pid)) {
		PRINTF("CLT cannot detach name\n");
	}

	while (rt_get_adr(nam2num("SRV"))) {
		rt_sleep(nano2count(1000000));
	}
	if (rt_task_delete(clt)) {
		PRINTF("CLT cannot delete task\n");
	}

	stop_rt_timer();
	return 0;
}
