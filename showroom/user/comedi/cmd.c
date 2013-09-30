/*
COPYRIGHT (C) 2009  Edoardo Vigoni   (vigoni@aero.polimi.it)
COPYRIGHT (C) 2009  Paolo Mantegazza <mantegazza@aero.polimi.it>

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

#include <rtai_comedi.h>

#define ONECALL    1
#define TIMEDCALL  1

#define TIMEOUT  100000000

#define NCHAN  5

#define SAMP_FREQ  10000
#define RUN_TIME   1

#define AI_RANGE  0
#define SAMP_TIME  (1000000000/SAMP_FREQ)
static comedi_t *dev;
static int subdev;
static comedi_krange krange;
static lsampl_t maxdata;

static int init_board(void)
{
	dev = comedi_open("/dev/comedi0");
	printf("Comedi device (6071) handle: %p.\n", dev);
	if (!dev){
		printf("Unable to open (6071) %s.\n", "/dev/comedi0");
		return 1;
	}
	subdev = comedi_find_subdevice_by_type(dev, COMEDI_SUBD_AI, 0);
	if (subdev < 0) {
		comedi_close(dev);
		printf("Subdev (6071) %d not found.\n", COMEDI_SUBD_AI);
		return 1;
	}
	comedi_get_krange(dev, subdev, 0, AI_RANGE, &krange);
	maxdata = comedi_get_maxdata(dev, subdev, 0);
	return 0;
}

int do_cmd(void)
{
	int ret, i;
	comedi_cmd cmd;
	unsigned int chanlist[NCHAN];
	unsigned int buf_read[NCHAN] = { 2, 3, 4, 5, 6 };

	memset(&cmd, 0, sizeof(cmd));
	for (i = 0; i < NCHAN; i++) {
		chanlist[i] = CR_PACK(buf_read[i], AI_RANGE, AREF_GROUND);
	}

	cmd.subdev = subdev;
	cmd.flags = TRIG_RT | TRIG_WAKE_EOS;

	cmd.start_src = TRIG_NOW;
	cmd.start_arg = 0;

	cmd.scan_begin_src = TRIG_TIMER;
	cmd.scan_begin_arg = SAMP_TIME;

	cmd.convert_src = TRIG_TIMER;
	cmd.convert_arg = 2000;

	cmd.scan_end_src = TRIG_COUNT;
	cmd.scan_end_arg = NCHAN;

	cmd.stop_src = TRIG_NONE;
	cmd.stop_arg = 0;

	cmd.chanlist = chanlist;
	cmd.chanlist_len = NCHAN;

	ret = comedi_command_test(dev, &cmd);
	printf("1st comedi_command_test returned: %d.\n", ret);
	ret = comedi_command_test(dev, &cmd);
	printf("2nd comedi_command_test returned: %d.\n", ret);
	printf("CONVERT ARG: %d\n", cmd.convert_arg);

	if (ret) {
		return ret;
	}

	ret = comedi_command(dev, &cmd);
	printf("Comedi_command returned: %d.\n", ret);

	return ret;
}

static volatile int end;
void endme(int sig) { end = 1; }

int main(void)
{
	RT_TASK *task;

	lsampl_t *hist;
	lsampl_t data[NCHAN] = { 0 };
	unsigned int val;
	long i, k, n, cnt = 0, retval = 0;
	FILE *fp;

	signal(SIGKILL, endme);
	signal(SIGTERM, endme);
	hist = malloc(SAMP_FREQ*RUN_TIME*NCHAN*sizeof(lsampl_t) + 1000);
	memset(hist, 0, SAMP_FREQ*RUN_TIME*NCHAN*sizeof(lsampl_t) + 1000);

	start_rt_timer(0);
	task = rt_task_init_schmod(nam2num("MYTASK"), 1, 0, 0, SCHED_FIFO, 0xF);
	printf("COMEDI CMD TEST BEGINS: SAMPLING FREQ: %d, RUN TIME: %d.\n", SAMP_FREQ, RUN_TIME);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	if (init_board()) {;
		printf("Board initialization failed.\n");
		return 1;
	}
	rt_comedi_register_callback(dev, subdev, COMEDI_CB_EOS, NULL, task);
	do_cmd();

	for (n = k = 0; k < SAMP_FREQ*RUN_TIME && !end; k++) {
#if ONECALL

		val = COMEDI_CB_EOS;
#if TIMEDCALL
		retval += rt_comedi_command_data_wread_timed(dev, subdev, NCHAN, data, nano2count(TIMEOUT), &val);
#else
		retval += rt_comedi_command_data_wread(dev, subdev, NCHAN, data,&val);
#endif

#else

		val = 0;
#if TIMEDCALL
		retval += rt_comedi_wait_timed(nano2count(TIMEOUT), &val);
#else
		retval += rt_comedi_wait(&val);
#endif

#endif
		if (val & COMEDI_CB_EOS) {
#if !ONECALL
			rt_comedi_command_data_read(dev, subdev, NCHAN, data);
#endif
//			printf("Read %ld: %u.\n", k, data[0]);
			for (i = 0; i < NCHAN; i++) {
				 hist[n++] = data[i];
			}
		} else {
			printf("Callback mask does not match: %lu.\n", ++cnt);
		}
	}

	comedi_cancel(dev, subdev);
	comedi_close(dev);
	printf("COMEDI TEST ENDS.\n");

	if (retval < 0) {
		printf("rt_comedi_wait_timed overruns: %d\n", abs(retval));
	}
	fp = fopen("rec.dat", "w");
	for (n = k = 0; k < SAMP_FREQ*RUN_TIME; k++) {
		fprintf(fp, "# %ld: ", k);
		for (i = 0; i < NCHAN; i++) {
			fprintf(fp, "%d\t", hist[n++]);
		}
		fprintf(fp, "\n");
	}
	fclose(fp);
	free(hist);

	stop_rt_timer();
	rt_make_soft_real_time();
	rt_task_delete(task);

	return 0;
}
