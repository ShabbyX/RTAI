/*
Original example:
	 COPYRIGHT (C) 2002  Giuseppe Renoldi (giuseppe@renoldi.org)
Adaption to RTAI_TRIOSS using RTDM:
	COPYRIGHT (C) 2005  Paolo Mantegazza (mantegazza@aero.polimi.it)

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


/*
 * rtai-16550A test
 * ================
 *
 * Adaptation of rtai_spdrv test provided in RTAI.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/io.h>
#include <signal.h>

#include <rtai_sem.h>
#include <rtdm/rtdm.h>
#include <rtdm/rtserial.h>

#define PRINT rt_printk // or printf to test going back and forth

#define LOOPS        10000
#define TRX_TIMEOUT  2000000
#define BAUD_RATE    115200
#define PAUSE_TIME   4000

static const struct rtser_config read_config = {
    0xFFFF,                     /* config_mask */
    115200,                     /* baud_rate */
    RTSER_DEF_PARITY,           /* parity */
    RTSER_DEF_BITS,             /* data_bits */
    RTSER_DEF_STOPB,            /* stop_bits */
    RTSER_DEF_HAND,             /* handshake */
    RTSER_DEF_FIFO_DEPTH,       /* fifo_depth*/
    TRX_TIMEOUT,                /* rx_timeout */
    RTSER_DEF_TIMEOUT,          /* tx_timeout */
    TRX_TIMEOUT,                /* event_timeout */
    RTSER_RX_TIMESTAMP_HISTORY, /* timestamp_history */
    RTSER_EVENT_RXPEND          /* event mask */
};

static int sfd, rfd, end;

static void endme(int dummy)
{
	end = 1;
	rt_dev_close(sfd);
	rt_dev_close(rfd);
}

int main(void)
{
	RTIME te, ts, tr;
	RT_TASK *testcomtsk;
	char hello[] = "Hello World\n\r";
	rtser_config_t serconf;
	struct rtser_status status;
	int mcr_status, i;

	signal(SIGINT,  endme);
	signal(SIGKILL, endme);
	signal(SIGTERM, endme);

	if (!(testcomtsk = rt_task_init(nam2num("TESTCOM"), 1, 0, 0))) {
		printf("CANNOT INIT MASTER TASK\n");
		exit(1);
	}
	rt_set_oneshot_mode();
	start_rt_timer(0);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	if ((sfd = rt_dev_open("rtser0", O_RDWR)) < 0 || (rfd = rt_dev_open("rtser1", O_RDWR)) < 0) {
		PRINT("hello_world_lxrt: error in rt_dev_open()\n");
		return 1;
	} else {	
// GET_CONFIGs not needed, as the duplicated initializations, just for testing.
		rt_dev_ioctl(sfd, RTSER_RTIOC_GET_CONFIG, &serconf);
		serconf.config_mask = RTSER_SET_BAUD | RTSER_SET_TIMEOUT_TX;
		serconf.rx_timeout  = TRX_TIMEOUT;
		serconf.baud_rate   = BAUD_RATE;
		rt_dev_ioctl(sfd, RTSER_RTIOC_SET_CONFIG, &serconf);
		rt_dev_ioctl(rfd, RTSER_RTIOC_GET_CONFIG, &serconf);
		serconf.config_mask = RTSER_SET_BAUD | RTSER_SET_TIMEOUT_RX | RTSER_SET_TIMEOUT_EVENT;
		serconf.event_timeout  = 2000000000; //TRX_TIMEOUT;
		serconf.rx_timeout  = TRX_TIMEOUT;
		serconf.baud_rate   = BAUD_RATE;
		rt_dev_ioctl(rfd, RTSER_RTIOC_SET_CONFIG, &serconf);
		rt_dev_ioctl(rfd, RTSER_RTIOC_SET_CONFIG, &read_config);
		PRINT("\nhello_world_lxrt: rtser0 test started (fd_count = %d)\n", rt_dev_fdcount());
		te = rt_get_cpu_time_ns();
		for (i = 1; i <= LOOPS && !end; i++) {
			strcpy(hello, "Hello World\n\r");
			ts = rt_get_time_ns();
			rt_dev_write(sfd, hello, sizeof(hello) - 1);
			rt_dev_ioctl(sfd, RTSER_RTIOC_GET_STATUS, &status);
			PRINT("hello_world_lxrt: line status = 0x%x, modem status = 0x%x.\n", status.line_status, status.modem_status);
			rt_dev_ioctl(sfd, RTSER_RTIOC_GET_CONTROL, &mcr_status);
			rt_dev_ioctl(sfd, RTSER_RTIOC_SET_CONTROL, mcr_status);
			PRINT("hello_world_lxrt: modem control status = 0x%x.\n", mcr_status);
			rt_sleep(nano2count(PAUSE_TIME));
			PRINT("\nhello_world_lxrt: %d - SENT ON <rtser0>: >>%s<<.\n\n", i, hello);
			hello[0] = 0;
			PRINT("hello_world_lxrt: waiting to receive with timeout %llu (ns).\n", serconf.rx_timeout);
struct rtser_event rx_event;
			if (rt_dev_ioctl(rfd, RTSER_RTIOC_WAIT_EVENT, &rx_event )) {
				PRINT("RCV EVENT FAILED\n");
			}
			rt_dev_read(rfd, hello, sizeof(hello) - 1);
			tr = rt_get_time_ns();
			PRINT("\nhello_world_lxrt: %d - RECEIVED ON <rtser1>: >>%s<< [ %lld + %lld = %lld (us) ].\n\n", i, hello, (rx_event.rxpend_timestamp - ts)/1000, (tr - rx_event.rxpend_timestamp)/1000, (tr - ts)/1000);
		}
		rt_printk("EXECT TIME: %lld (ms)\n", (rt_get_cpu_time_ns() - te)/1000000);
		rt_sleep(nano2count(PAUSE_TIME));
		rt_dev_close(sfd);
		rt_dev_close(rfd);
		PRINT("hello_world_lxrt: wait event (forced to be wrong) - 0x%x\n",
		rt_dev_ioctl(1000000, RTSER_RTIOC_WAIT_EVENT, NULL)
		);
		PRINT("hello_world_lxrt: rtser0 and rtser1 test finished\n");
	}

	rt_make_soft_real_time();
	rt_task_delete(testcomtsk);
	stop_rt_timer();
	return 0;
}
