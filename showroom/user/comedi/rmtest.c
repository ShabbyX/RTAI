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


/*
 * This program does nothing but verifying the RTAI netrpc interface
 * to COMEDI services in terms of passing call and return parameters.
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
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rtai_comedi.h>

//#define LOCAL_EXEC

#define PRINT printf

#define TIMEOUT  2000000000LL

#define DATA_READ_DELAY  2000000000

#define NICHAN  5
#define NOCHAN  2
#define NCHAN   (NICHAN + NOCHAN)

#define AI_RANGE  0
#define AO_RANGE  0

#define SAMP_TIME 1000

static comedi_t *dev;
static unsigned int subdevai, subdevao, subdevdg;
static comedi_krange krangeai, krangeao;
static lsampl_t maxdatai, maxdatao;
static unsigned int daqnode, daqport;

static int test_init_board(void)
{
	dev = RT_comedi_open(daqnode, daqport, "/dev/comedi");		
	PRINT("Comedi device handle: %p.\n", dev);

	subdevai = RT_comedi_find_subdevice_by_type(daqnode, daqport, dev, COMEDI_SUBD_AI, 0);
	PRINT("Comedi anolog input subdevice: %u.\n", subdevai);
	RT_comedi_get_krange(daqnode, daqport, dev, subdevai, 0, AI_RANGE, &krangeai);
	PRINT("Comedi analog input krange: %d %d %u.\n", krangeai.min, krangeai.max, krangeai.flags);
	maxdatai = RT_comedi_get_maxdata(daqnode, daqport, dev, subdevai, 0);
	PRINT("Comedi analog input maxdata: %d.\n", maxdatai);

	subdevao = RT_comedi_find_subdevice_by_type(daqnode, daqport, dev, COMEDI_SUBD_AO, 0);
	PRINT("Comedi anolog output subdevice: %u.\n", subdevao);
	RT_comedi_get_krange(daqnode, daqport, dev, subdevao, 0, AO_RANGE, &krangeao);
	PRINT("Comedi analog input krange: %d %d %u.\n", krangeao.min, krangeao.max, krangeao.flags);
	maxdatao = RT_comedi_get_maxdata(daqnode, daqport, dev, subdevao, 0);
	PRINT("Comedi analog output maxdata: %d.\n", maxdatao);

	subdevdg = RT_comedi_find_subdevice_by_type(daqnode, daqport, dev, COMEDI_SUBD_AO, 0);
	PRINT("Comedi digital IO subdevice: %u.\n", subdevdg);
	return 0;
}

int test_cmd(void)
{
	int ret, i;
	comedi_cmd cmd;
	unsigned int chanlist[NCHAN];
	unsigned int buf_read[NCHAN] = { 2, 3, 4, 5, 6 };

	memset(&cmd, 0, sizeof(cmd));
	for (i = 0; i < NCHAN; i++) {
		chanlist[i] = CR_PACK(buf_read[i], AI_RANGE, AREF_GROUND);
	}

	cmd.subdev = subdevai;
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

	PRINT("SENT COMMAND, subdev = %u, flags = %u, start_src = %u, start_arg = %u, scan_begin_src = %u, scan_begin_arg = %u, convert_src = %u, convert_arg = %u, scan_end_src = %u, scan_end_arg = %u, stop_src = %u, stop_arg = %u, chanlist_len = %u.\n", cmd.subdev, cmd.flags, cmd.start_src, cmd.start_arg, cmd.scan_begin_src, cmd.scan_begin_arg, cmd.convert_src, cmd.convert_arg, cmd.scan_end_src, cmd.scan_end_arg, cmd.stop_src, cmd.stop_arg, cmd.chanlist_len);
	for (i = 0; i < NCHAN; i++) {
		PRINT("CHALIST, # %d, val = %u.\n", i, chanlist[i]);
	}

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

	ret = RT_comedi_command_test(daqnode, daqport, dev, &cmd);
	PRINT("Comedi_command_test returned: %d.\n", ret);
	ret = RT_comedi_command(daqnode, daqport, dev, &cmd);
	PRINT("Comedi_command returned: %d.\n", ret);

	return 0;
}

static volatile int end;
void endme(int sig) { end = 1; }

int main(int argc, char *argv[])
{
	RT_TASK *task;
	lsampl_t data[NCHAN] = { 0 };
	unsigned int val, chan;
	long i;

	comedi_insn insn[NCHAN];
        unsigned int read_chan[NICHAN]  = { 2, 3, 4, 5, 6 };
        unsigned int write_chan[NOCHAN] = { 0, 1 };
	comedi_insnlist ilist = { NCHAN, insn };

	struct sockaddr_in addr;

	signal(SIGKILL, endme);
	signal(SIGTERM, endme);

	start_rt_timer(0);
	task = rt_task_init_schmod(nam2num("MYTASK"), 1, 0, 0, SCHED_FIFO, 0xF);

	daqnode = 0;
#ifdef LOCAL_EXEC
#else
	if (argc == 2 && strstr(argv[1], "DaqNode=")) {
		inet_aton(argv[1] + 8, &addr.sin_addr);
		daqnode = addr.sin_addr.s_addr;
	}
	if (!daqnode) {
		inet_aton("127.0.0.1", &addr.sin_addr);
		daqnode = addr.sin_addr.s_addr;
	}
        while ((daqport = rt_request_port(daqnode)) <= 0 && daqport != -EINVAL);
#endif
	printf("REMOTE COMEDI TEST BEGINS:  NODE = %x, PORT = %d.\n", daqnode, daqport);

	test_init_board();

	RT_comedi_register_callback(daqnode, daqport, dev, subdevai, COMEDI_CB_EOS, NULL, task);
	PRINT("Comedi analog input registered a call back.\n");
	
	test_cmd();

	val = COMEDI_CB_EOS;
	PRINT("COMMAND WREAD TIMED with a %lld (ns) timeout.\n", TIMEOUT);
	RT_comedi_command_data_wread_timed(daqnode, daqport, dev, subdevai, NCHAN, data, TIMEOUT, &val);
	PRINT("COMMAND WREAD TIMED, val = %x.\n", val);
	for (i = 0; i < NCHAN; i++) {
		PRINT("CHAN # %ld, data = %x.\n", i, data[i]);
	}
	RT_comedi_get_driver_name(daqnode, daqport, dev, (char *)data);
	PRINT("GET DRIVER NAME returned %s.\n", (char *)data);
	RT_comedi_command_data_wread(daqnode, daqport, dev, subdevai, NCHAN, data, &val);
	PRINT("COMMAND WREAD, val = %x.\n", val);
	for (i = 0; i < NCHAN; i++) {
		PRINT("CHAN # %ld, data = %x.\n", i, data[i]);
	}
	PRINT("COMMAND WAIT TIMED with a %lld (ns) timeout.\n", TIMEOUT);
	RT_comedi_wait_timed(daqnode, daqport, TIMEOUT, &val);
	PRINT("COMEDI WAIT TIMED, val = %x.\n", val);
	RT_comedi_wait(daqnode, daqport, &val);
	PRINT("COMEDI WAIT, val = %x.\n", val);
	RT_comedi_command_data_read(daqnode, daqport, dev, subdevai, NCHAN, data);
	PRINT("COMMAND READ.\n");
	for (i = 0; i < NCHAN; i++) {
		PRINT("CHAN # %ld, data = %x.\n", i, data[i]);
	}

	PRINT("COMMAND WRITE.\n");
	for (i = 0; i < NCHAN; i++) {
		data[i] = 0xcba + i;
		PRINT("CHAN # %ld, data = %x.\n", i, data[i]);
	}
	rt_comedi_command_data_write(dev, subdevao, NCHAN, data);
	PRINT("COMMAND TRIGGER.\n");
	rt_comedi_trigger(dev, subdevai);

	RT_comedi_data_write(daqnode, daqport, dev, subdevai, 0, 0, AREF_GROUND, 2048);
	PRINT("DATA WRITE: dev = %p, subdev = %u, chan = %u, range = %u, aref = %x, data = %u.\n", dev, subdevai, 0, 0, AREF_GROUND, 2048);
	RT_comedi_data_read(daqnode, daqport, dev, subdevai, 0, 0, AREF_GROUND, data);
	PRINT("DATA READ: dev = %p, subdev = %u, chan = %u, range = %u, aref = %x, data = %x.\n", dev, subdevao, 0, 0, AREF_GROUND, data[0]);
	PRINT("DATA READING DELAYED: dev = %p, subdev = %u, chan = %u, range = %u, aref = %x, data = %x, delay = %u (ns).\n", dev, subdevao, 0, 0, AREF_GROUND, data[0], DATA_READ_DELAY);
	RT_comedi_data_read_delayed(daqnode, daqport, dev, subdevai, 0, 0, AREF_GROUND, data, DATA_READ_DELAY);
	PRINT("DATA READ DELAYED: dev = %p, subdev = %u, chan = %u, range = %u, aref = %x, data = %x.\n", dev, subdevao, 0, 0, AREF_GROUND, data[0]);

	data[0] = 0x77777777;
	data[1] = 0x77777777;
	PRINT("DIO BITFIELD: dev = %p, subdev = %u, write_mask = %x, bits = %x.\n", dev, subdevdg, data[1], data[0]);
	RT_comedi_dio_bitfield(daqnode, daqport, dev, subdevdg, data[1], data);
	PRINT("DIO BITFIELD: dev = %p, subdev = %u, write_mask = %x, bits = %x.\n", dev, subdevdg, data[1], data[0]);

	val = 0x77777777;
	chan = 57;
	PRINT("DIO WRITE: dev = %p, subdev = %u, chan = %u, val= %x.\n", dev, subdevdg, chan, val);
	RT_comedi_dio_write(daqnode, daqport, dev, subdevdg, chan, val);
	PRINT("DIO WRITE: dev = %p, subdev = %u, chan = %u, val= %x.\n", dev, subdevdg, chan, val);

	PRINT("DIO READ: dev = %p, subdev = %u, chan = %u.\n", dev, subdevdg, chan);
	RT_comedi_dio_read(daqnode, daqport, dev, subdevdg, chan, &val);
	PRINT("DIO READ: dev = %p, subdev = %u, chan = %u, val= %x.\n", dev, subdevdg, chan, val);

        for (i = 0; i < NICHAN; i++) {
		data[i] = 1000 + i;
		BUILD_AREAD_INSN(insn[i], subdevai, data[i], 1, read_chan[i], AI_RANGE, AREF_GROUND);
		PRINT("READ INSN: insn = %u, n = %u, data = %u, subdev = %u, chanspec %u.\n", insn[i].insn, insn[i].n, insn[i].data[0], insn[i].subdev, insn[i].chanspec);
        }

        for (i = 0; i < NOCHAN; i++) {
		data[NICHAN + i] = 1000 + i;
		BUILD_AWRITE_INSN(insn[NICHAN + i], subdevao, data[NICHAN + i], 1, write_chan[i], AO_RANGE, AREF_GROUND);
		PRINT("WRIT INSN: insn = %u, n = %u, data = %u, subdev = %u, chanspec = %u.\n", insn[NICHAN + i].insn, insn[NICHAN + i].n, insn[NICHAN + i].data[0], insn[NICHAN + i].subdev, insn[NICHAN + i].chanspec);
        }

	for (i = 0; i < NCHAN; i++) {
		PRINT("1 COMEDI INST: insn = %u, n = %u, data = %u, subdev = %u, chanspec = %u.\n", insn[i].insn, insn[i].n, insn[i].data[0], insn[i].subdev, insn[i].chanspec);
		PRINT("COMEDI INST: %d.\n", RT_comedi_do_insn(daqnode, daqport, dev, insn + i));
		PRINT("2 COMEDI INST: insn = %u, n = %u, data = %x, subdev = %u, chanspec = %u.\n", insn[i].insn, insn[i].n, insn[i].data[0], insn[i].subdev, insn[i].chanspec);
		insn[i].n = 1;
		insn[i].data[0] = 999999;
	}
	PRINT("COMEDI INSTLST: %d.\n", RT_comedi_do_insnlist(daqnode, daqport, dev, &ilist));
	for (i = 0; i < NCHAN; i++) {
		PRINT("COMEDI INSTLST: insn = %u, n = %u, data = %x, subdev = %u, chanspec = %u.\n", insn[i].insn, insn[i].n, insn[i].data[0], insn[i].subdev, insn[i].chanspec);
	}

	RT_comedi_cancel(daqnode, daqport, dev, subdevai);
	RT_comedi_cancel(daqnode, daqport, dev, subdevao);
	RT_comedi_cancel(daqnode, daqport, dev, subdevdg);
	PRINT("COMEDI CANCELs DONE.\n");
	RT_comedi_close(daqnode, daqport, dev);
	PRINT("COMEDI CLOSE.\n");

	stop_rt_timer();
	rt_task_delete(task);

	return 0;
}
