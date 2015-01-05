/* select.c
 *
 * select - an example demonstrating the use of select within RTNet
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
 This example might appear a bit bloated as, being based on threads, it 
 could have shared anything. We have elected to not use global data just
 to show a few more RTAI services working together, e.g.: synchronization
 and timing through intertask messages and objects handles through registered
 infos.
*/

#include <pthread.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>

#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <rtai_mbx.h>
#include <rtai_msg.h>

#include <rtnet.h>

#define USESEL    1
#define USEMBX    1
#define CPUMAP    0xF
#define WORKCYCLE 10000000LL
#define TIMEOUT   (WORKCYCLE*25)/100

#define ECHO rt_printk
//#define ECHO printf

#define PORT 55555

struct sample { unsigned long cnt; RTIME tx, rx; };

static void *sender(void *arg)
{
	RT_TASK *Sender_Task;
	unsigned long sock, slen;
	struct sockaddr_in transmit_addr;
	RTIME until;
	struct sample tx_samp = { 0, };
	int diff, max = -100000000;

	if (!(Sender_Task = rt_thread_init(nam2num("TXTSK"), 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("Cannot initialise the sender task\n");
		exit(1);
	}

	if (((sock = rt_dev_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))) == -1) {
		printf("Error opening UDP/IP socket: %lu\n", sock);
		exit(1);
	}

	transmit_addr.sin_family      = AF_INET;
	transmit_addr.sin_port        = htons(PORT);
	transmit_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	rt_rpc(rt_get_adr(nam2num("MNTSK")), 0UL, &slen);

	ECHO("Transmitter task initialised\n");

	rt_make_hard_real_time();

	until = rt_get_time();
	while(rt_receive_until(rt_get_adr(nam2num("MNTSK")), &slen, until += nano2count(WORKCYCLE)) != rt_get_adr(nam2num("MNTSK"))) {
		diff = count2nano(abs((int)(until - rt_get_time())));
		if (diff > max) max = diff;
		rt_printk("LATENCY %d, max = %d\n", diff, max);
		++tx_samp.cnt;
		tx_samp.tx = rt_get_real_time_ns();
		slen = rt_dev_sendto(sock, (void *)&tx_samp, offsetof(struct sample, rx), 0, (struct sockaddr*)&transmit_addr, sizeof(transmit_addr));
		if (slen == offsetof(struct sample, rx)) {
			ECHO("TRASMITTED %lu %ld %lld\n", slen, tx_samp.cnt, tx_samp.tx/1000);
		} else {
			ECHO("SENDER SENT %lu INSTEAD OF %d\n", slen, offsetof(struct sample, rx));
		}
//		rt_sleep_until(until += nano2count(WORKCYCLE)); timing cared by the timed while
	}

	rt_make_soft_real_time();
	rt_dev_close(sock);
	rt_dev_shutdown(sock, SHUT_RDWR);
	rt_return(rt_get_adr(nam2num("MNTSK")), 0UL);
	rt_thread_delete(Sender_Task);

	printf("Transmitter exiting\n");

	return 0;
}

static void *receiver(void *arg)
{
	RT_TASK *Receiver_Task;
	unsigned long sock, rlen;
	int broadcast = 1;
	struct sockaddr_in local_addr;
	socklen_t fromlen;
	struct sockaddr_in receive_addr;
	fromlen = sizeof(receive_addr);
	fd_set rxfds;
	struct sample rx_samp;
	long cnt = 0;
	MBX *mbx = rt_get_adr(nam2num("MYMBX"));

	if (!(Receiver_Task = rt_thread_init(nam2num("RXTSK"), 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("Cannot initialise the receiver task\n");
		exit(1);
	}

	if (((sock = rt_dev_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))) == -1) {
		printf("Error opening UDP/IP socket: %lu\n", sock);
		exit(1);
	}
	if (rt_dev_setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == -1) {
		printf("Can't set broadcast options\n");
		rt_dev_close(sock);
		exit(1);
	}
	if (USESEL) {
		int64_t timeout = -1;
		rt_dev_ioctl(sock, RTNET_RTIOC_TIMEOUT, &timeout);
	}

	local_addr.sin_family      = AF_INET;
	local_addr.sin_port        = htons(PORT);
	local_addr.sin_addr.s_addr = INADDR_ANY;
	if (rt_dev_bind(sock, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_in)) == -1) {
		ECHO("Can't configure the network socket");
		rt_dev_close(sock);
		exit(1);
	}

	rt_rpc(rt_get_adr(nam2num("MNTSK")), 0UL, &rlen);

	ECHO("Receiver task initialised\n");

	rt_make_hard_real_time();

	while(!rt_receive_if(rt_get_adr(nam2num("MNTSK")), &rlen)) {
		int ready = 0;
#if USESEL
		static int usetimeout = 0;
		RTIME timeout;
		usetimeout = 1 - usetimeout;
		timeout = usetimeout ? TIMEOUT : 0;
		FD_ZERO(&rxfds);
		FD_SET(sock, &rxfds);
		ready = rt_dev_select(sock + 1, &rxfds, NULL, NULL, timeout);
		ECHO("Receiver select returned %d (0 is TIMEDOUT)\n", ready);
		if (ready <= 0) {
			continue;
		}
#endif
		if (!USESEL || (ready > 0 && FD_ISSET(sock, &rxfds))) {
			rlen = rt_dev_recvfrom(sock, (void *)&rx_samp, sizeof(struct sample), 0, (struct sockaddr*)&receive_addr, &fromlen);
			if (rlen == offsetof(struct sample, rx)) {
				rx_samp.rx = rt_get_real_time_ns();
				rt_mbx_send_if(mbx, &rx_samp, sizeof(rx_samp));
				ECHO("RECEIVED %lu %ld-%ld %lld %lld\n", rlen, ++cnt, rx_samp.cnt, rx_samp.tx/1000, rx_samp.rx/1000);
			} else {
				ECHO("RECEIVER EXPECTED %d GOT %lu\n", sizeof(rx_samp), rlen);
			}
		}
	}

	rt_make_soft_real_time();
	rt_dev_close(sock);
	rt_dev_shutdown(sock, SHUT_RDWR);
	rt_return(rt_get_adr(nam2num("MNTSK")), 0UL);
	rt_thread_delete(Receiver_Task);

	ECHO("Receiver exiting\n");

	return 0;
}

volatile int end;
static void endme(int unused) { end = 1; }

int main(void)
{
	RT_TASK *Main_Task;
	MBX *mbx;
	pthread_t sender_thread;
	pthread_t receiver_thread;
	struct sample frombx;
	unsigned long cnt = 0, misd;

	signal(SIGHUP, endme);
	signal(SIGINT, endme);
	signal(SIGKILL, endme);
	signal(SIGTERM, endme);
	signal(SIGALRM, endme);

	rt_allow_nonroot_hrt();
	start_rt_timer(0);

	if (!(Main_Task = rt_thread_init(nam2num("MNTSK"), 0, 0, SCHED_FIFO, 0xF))) {
		printf("Cannot initialise the main task\n");
		exit(1);
	}

	if (!(mbx = rt_mbx_init(nam2num("MYMBX"), 10*sizeof(struct sample)))) {
		printf("Cannot create the mailbox\n");
		exit(1);
	}

	sender_thread = rt_thread_create(sender, NULL, 0);
	rt_return(rt_receive(NULL, &cnt), 0UL); 

	receiver_thread = rt_thread_create(receiver, NULL, 0);
	rt_return(rt_receive(NULL, &cnt), 0UL); 

	mlockall(MCL_CURRENT | MCL_FUTURE);

	rt_make_hard_real_time();

#if USEMBX
	while(!end) {
		if (!(misd = rt_mbx_receive(mbx, (void *)&frombx, sizeof(frombx)))) {
			ECHO("MAIN FROM MBX %lu-%lu %lld %lld\n", ++cnt, frombx.cnt, frombx.tx/1000, frombx.rx/1000);
		} else {
			ECHO("MAIN MBX ASKED FOR %d MISSED %lu\n", sizeof(frombx), misd);
		}
	}
#else
	pause();
#endif

	rt_task_masked_unblock(rt_get_adr(nam2num("RXTSK")), ~RT_SCHED_READY);
	rt_rpc(rt_get_adr(nam2num("RXTSK")), 0UL, &cnt); 
	rt_rpc(rt_get_adr(nam2num("TXTSK")), 0UL, &cnt); 
	rt_thread_join(sender_thread);
	rt_thread_join(receiver_thread);

	stop_rt_timer();
	rt_make_soft_real_time();
	rt_mbx_delete(mbx);
	rt_thread_delete(Main_Task);

	return 0;
}
