/*
COPYRIGHT (C) 2010  Edoardo Vigoni   (vigoni@aero.polimi.it)
COPYRIGHT (C) 2010  Paolo Mantegazza (mantegazza@aero.polimi.it)

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
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>
#include <math.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rtai_comedi.h>

#define NCHAN  2

#define SIN_FREQ  200
#define N_PNT     50
#define RUN_TIME  5

#define AO_RANGE  0
#define SAMP_FREQ  (SIN_FREQ*N_PNT)
#define SAMP_TIME  (1000000000/SAMP_FREQ)
#define TRIGSAMP  2048

static comedi_t *dev;
static int subdev;
static comedi_krange krange;
static lsampl_t maxdata;
unsigned int daqnode, daqport;

static int init_board(void)
{
	dev = RT_comedi_open(daqnode, daqport, "/dev/comedi0");
	printf("Comedi device (6071) handle: %p.\n", dev);
	if (!dev){
		printf("Unable to open (6071) %s.\n", "/dev/comedi0");
		return 1;
	}
	subdev = RT_comedi_find_subdevice_by_type(daqnode, daqport, dev, COMEDI_SUBD_AO, 0);
	if (subdev < 0) {
		RT_comedi_close(daqnode, daqport, dev);
		printf("Subdev (6071) %d not found.\n", COMEDI_SUBD_AO);
		return 1;
	}
	RT_comedi_get_krange(daqnode, daqport, dev, subdev, 0, AO_RANGE, &krange);
	maxdata = RT_comedi_get_maxdata(daqnode, daqport, dev, subdev, 0);
	return 0;
}

	comedi_cmd cmd;
int do_cmd(void)
{
	int ret, i;
	unsigned int chanlist[NCHAN];
	unsigned int buf_write[NCHAN] = { 0, 1 };

	memset(&cmd, 0, sizeof(cmd));
	for (i = 0; i < NCHAN; i++) {
		chanlist[i] = CR_PACK(buf_write[i], AO_RANGE, AREF_GROUND);
	}

	cmd.subdev = subdev;
	cmd.flags = TRIG_RT | TRIG_WRITE;

	cmd.start_src = TRIG_INT;
	cmd.start_arg = 0;

	cmd.scan_begin_src = TRIG_TIMER;
	cmd.scan_begin_arg = SAMP_TIME;

	cmd.convert_src = TRIG_NOW;
	cmd.convert_arg = 0;

	cmd.scan_end_src = TRIG_COUNT;
	cmd.scan_end_arg = NCHAN;

	cmd.stop_src = TRIG_NONE;
	cmd.stop_arg = 0;

	cmd.chanlist = chanlist;
	cmd.chanlist_len = NCHAN;

	ret = RT_comedi_command_test(daqnode, daqport, dev, &cmd);
	ret = RT_comedi_command_test(daqnode, daqport, dev, &cmd);

	if (ret) {
		return ret;
	}

	ret = RT_comedi_command(daqnode, daqport, dev, &cmd);

	return ret;
}

static volatile int end;
void endme(int sig) { end = 1; }

int main(int argc, char *argv[])
{
	double omega = (2.0*M_PI*SIN_FREQ*SAMP_TIME)/1.0E9;
	RTIME until;
	RT_TASK *task;
	struct sockaddr_in addr;

	lsampl_t data[NCHAN*2];
	long k, sinewave, retval = 0;

	signal(SIGKILL, endme);
	signal(SIGTERM, endme);

	start_rt_timer(0);
	task = rt_task_init_schmod(nam2num("MYTASK"), 1, 0, 0, SCHED_FIFO, 0xF);

	daqnode = 0;
	if (argc == 2 && strstr(argv[1], "RcvNode=")) {
		inet_aton(argv[1] + 8, &addr.sin_addr);
		daqnode = addr.sin_addr.s_addr;
	}
	if (!daqnode) {
		inet_aton("127.0.0.1", &addr.sin_addr);
		daqnode = addr.sin_addr.s_addr;
	}
	while ((daqport = rt_request_port(daqnode)) <= 0 && daqport != -EINVAL);

	mlockall(MCL_CURRENT | MCL_FUTURE);
	printf("COMEDI CMD TEST BEGINS: SAMPLING FREQ: %d, RUN TIME: %d, NODE: %x, PORT: %d.\n", SAMP_FREQ, RUN_TIME, daqnode, daqport);
	rt_make_hard_real_time();

	if (init_board()) {;
		printf("Board initialization failed.\n");
		return 1;
	}
	do_cmd();

	until = rt_get_cpu_time_ns() + (long long)RUN_TIME*1000000000;
	for (k = 0; k < SAMP_FREQ*RUN_TIME && !end; k++) {
		sinewave =  (long)(maxdata/4*sin(k*omega));
		data[0] = (lsampl_t)(  sinewave + maxdata/2);
		data[1] = (lsampl_t)(- sinewave + maxdata/2);
		while (RT_comedi_command_data_write(daqnode, daqport, dev, subdev, NCHAN, data) != NCHAN) {
			rt_sleep(nano2count(SAMP_TIME/2));
		}
		if (k == TRIGSAMP) {
			RT_comedi_trigger(daqnode, daqport, dev, subdev);
		}
	}

	while (until > rt_get_cpu_time_ns()) {
		rt_sleep(nano2count(100000));
	}
	RT_comedi_cancel(daqnode, daqport, dev, subdev);
	RT_comedi_close(daqnode, daqport, dev);
	RT_comedi_data_write(daqnode, daqport, dev, subdev, 0, 0, AREF_GROUND, 2048);
	RT_comedi_data_write(daqnode, daqport, dev, subdev, 1, 0, AREF_GROUND, 2048);
	printf("COMEDI TEST ENDS.\n");

	if (retval < 0) {
		printf("RT_comedi_wait_timed overruns: %d\n", abs(retval));
	}

	stop_rt_timer();
	rt_make_soft_real_time();
	rt_task_delete(task);

	return 0;
}
