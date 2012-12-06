/*
 * COPYRIGHT (C) 2008  Bernhard Pfund (bernhard@chapter7.ch)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
*/


/* 
 * RtnetTest core module
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/in.h>

#include <rtai_mbx.h>
#include </usr/rtnet/include/rtnet.h>

#define UDPPORT 55555
#define WORKCYCLE 100000
#define CPU 1
#define STKSIZ 8192

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bernhard Pfund <bernhard@chapter7.ch>");
MODULE_DESCRIPTION("RtnetTest core module");

static RT_TASK rx_task, tx_task;
static struct sockaddr_in loc_addr, rx_addr, tx_addr;

char buffer_in[1024];
char buffer_out[1024];
char buffer_time[256];
static int sock;
static int slen;
static int rlen;

static MBX* mbx;
struct sample { unsigned long cnt; RTIME tx, rx; } rx_samp;

static int end = 0;

/* This is the real period, returned by start_rt_timer */
static long period = 0;

static void receiver(long nothing)
{
	fd_set rxfds;
	RTIME timeout = 0;
	int ready = 0;
	socklen_t fromlen = sizeof(rx_addr);

	rt_printk("RtnetTest: Receiver task initialised\n");

	while(!end) {
		FD_ZERO(&rxfds);
		FD_SET(sock, &rxfds);
		ready = 0;

		ready = rt_dev_select(sock + 1, &rxfds, NULL, NULL, timeout);
		if (ready > 0 && FD_ISSET(sock, &rxfds)) {

			rlen = rt_dev_recvfrom(sock, buffer_in, sizeof(buffer_in), 0, (struct sockaddr*) &rx_addr, &fromlen);

			if (rlen > 0) {
				rx_samp.cnt++;
//				rx_samp.tx = simple_strtoll(buffer_in, NULL, 10);
				sscanf(buffer_in, "%lld", &rx_samp.tx);
				rx_samp.rx = rt_get_real_time_ns();
				rt_mbx_send_if(mbx, &rx_samp, sizeof(rx_samp));
				memset(buffer_in, 0, sizeof(buffer_in));
			}

		} else if (EPERM == -ready) {
			rt_printk("RtnetTest: Failed to rt_dev_select on socket\n");
			end = 1;
			break;

		} else if (EINTR == -ready) {
			rt_printk("RtnetTest: rt_dev_select was interrupted\n");
			end = 1;
			break;
		}
	}

	return;
}

static void sender(long nothing)
{
	int diff = 0, warmup = 1000000000/WORKCYCLE;
	RTIME t, tb;
	struct sample { unsigned long cnt; RTIME tx, rx; } samp = { 0, 0, 0 };
	rt_printk("RtnetTest: Transmitter task initialised\n");

	tb = t = rt_get_real_time_ns();
	while(!end) {
		slen = sprintf(buffer_out, "%lld", t);
		slen = rt_dev_sendto(sock, buffer_out, slen, 0, (struct sockaddr*)&tx_addr, sizeof(tx_addr));

		if (slen < 0) {
			rt_printk("RtnetTest: Packet send failed! Errno %d\n", -slen);
			return;
		}
		rt_task_wait_period();
		t = rt_get_real_time_ns();
		if (!warmup) {
			diff = abs((int)(t - tb - WORKCYCLE));
			samp.cnt++;
			tb = t;
			if (diff > samp.rx) samp.rx = diff;
			rt_mbx_send_if(mbx, &samp, sizeof(samp));
		} else {
			tb = rt_get_real_time_ns();
			warmup--;
		}
	}
}


static int _init(void)
{
	int broadcast = 1;
	int64_t timeout = -1;

	rt_printk("RtnetTest: Module initialisation started\n");

	memset(buffer_in, 0, sizeof(buffer_in));
	memset(&loc_addr, 0, sizeof (struct sockaddr_in));

	memset(buffer_out, 0, sizeof(buffer_out));
	memset(&tx_addr, 0, sizeof (struct sockaddr_in));

	loc_addr.sin_family      = AF_INET;
	loc_addr.sin_port        = htons(UDPPORT);
	loc_addr.sin_addr.s_addr = INADDR_ANY;

	tx_addr.sin_family       = AF_INET;
	tx_addr.sin_port         = htons(UDPPORT);
	tx_addr.sin_addr.s_addr  = rt_inet_aton("127.0.0.1");

	if (((mbx = rt_typed_named_mbx_init("MYMBX", 2000*sizeof(struct sample), FIFO_Q))) == NULL) {
		rt_printk("RtnetTest: Cannot create the mailbox\n");
		return -1;
	}

	if (((sock = rt_dev_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))) < 0) {
		rt_printk("RtnetTest: Error opening UDP/IP socket: %d\n", sock);
		return -1;
	}
	if (rt_dev_setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == -1) {
		rt_printk("RtnetTest: Can't set broadcast options\n");
		goto close_socks;
	}

	rt_dev_ioctl(sock, RTNET_RTIOC_TIMEOUT, &timeout);

	if (rt_dev_bind(sock, (struct sockaddr *) &loc_addr, sizeof(struct sockaddr_in)) < 0) {
		rt_printk("RtnetTest: Can't bind the network socket");
		goto close_socks;
	}

	rt_set_periodic_mode();
	period = start_rt_timer(nano2count(WORKCYCLE));

	if (rt_task_init_cpuid(&rx_task, receiver, 0, STKSIZ, 0, 0, 0, CPU) < 0) {
		rt_printk("RtnetTest: Can't initialise the receiver task");
		goto close_socks;
	}

	if (rt_task_init_cpuid(&tx_task, sender, 0, STKSIZ, 0, 0, 0, CPU) < 0) {
		rt_printk("RtnetTest: Can't initialise the transmitter task");
		goto close_socks;
	}

	if (0 != rt_task_make_periodic(&tx_task, rt_get_time() + 20*period, period)) {
		rt_printk("RtnetTest: Make sender periodic failed\n");
		goto close_socks;
	}

	if (rt_task_resume(&rx_task) < 0) {
		rt_printk("RtnetTest: Can't start the receiver task");
		goto close_socks;
	}

	rt_printk("RtnetTest: Module initialisation completed\n");
	return 0;

close_socks:

	rt_dev_close(sock);
	rt_dev_shutdown(sock, SHUT_RDWR);
	return -1;
}

static void _cleanup(void)
{
	rt_printk("RtnetTest: Module shutdown requested\n");

	end = 1;

	rt_task_delete(&rx_task);
	rt_task_delete(&tx_task);

	rt_dev_close(sock);
	rt_dev_shutdown(sock, SHUT_RDWR);

	rt_named_mbx_delete(mbx);

	rt_printk("RtnetTest: Module shutdown completed\n");
}

module_init(_init);
module_exit(_cleanup);
/*
 * EOF
 */
