/*
 * Copyright (C) 2010 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define DEV         0xcacca
#define SUBDEV      123
#define RANGE_MIN   -99
#define RANGE_MAX   +99
#define RANGE_FLAGS 0
#define MAXDATA     978
#define N_RANGES    3
#define N_CHANNELS  5
#define SUBDEV_TYPE 6
#define BOARD_NAME  "BOARD_NAME"
#define DRIVER_NAME "DRIVER_NAME"
#define CBMASK      0xFFFFFFFF
#define COMEDI_VERS 999

/*
 * This module serves to verify the RTAI netrpc interface to COMEDI
 * services in terms of passing call and return parameters.
 */

#include <linux/module.h>
#include <linux/version.h>

#include <rtai_schedcore.h>
#include <rtai_comedi.h>

#define MODULE_NAME "kcomedi_rt.ko"
MODULE_DESCRIPTION("Test to verify the RTAI netrpc interface to COMEDI");
MODULE_AUTHOR("Paolo Mantegazza");
MODULE_LICENSE("GPL");

#define RTAI_COMEDI_LOCK(dev, subdev) \
	do { _comedi_lock(dev, subdev); } while (0)
#define RTAI_COMEDI_UNLOCK(dev, subdev) \
	do { _comedi_unlock(dev, subdev); } while (0)

#define KSPACE(adr) ((unsigned long)adr > PAGE_OFFSET)

static RTAI_SYSCALL_MODE void *_comedi_open(const char *filename)
{
	rt_printk("COMEDI OPEN: NAME %s, (strlen = %d).\n", filename, strlen(filename));
	rt_printk("COMEDI OPEN RETURNS %x.\n", DEV);
	return (void *)DEV;
}

static RTAI_SYSCALL_MODE int _comedi_close(void *dev)
{
	rt_printk("COMEDI CLOSE: dev = %p.\n", dev);
	return 0;
}

static RTAI_SYSCALL_MODE int _comedi_lock(void *dev, unsigned int subdev)
{
	return 0;
}

static RTAI_SYSCALL_MODE int _comedi_unlock(void *dev, unsigned int subdev)
{
	return 0;
}

static RTAI_SYSCALL_MODE int _comedi_cancel(void *dev, unsigned int subdev)
{
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI CANCEL: dev = %p, subdev = %u.\n", dev, subdev);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return 0;
}

RTAI_SYSCALL_MODE int rt_comedi_register_callback(void *dev, unsigned int subdev, unsigned int mask, int (*callback)(unsigned int, void *), void *task)
{
	if (task == NULL) {
		task = rt_whoami();
	}
	((RT_TASK *)task)->resumsg = 0;
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI REGISTER CALLBACK: dev = %p, subdev = %u, mask = %u, callback = %p, task = %p.\n", dev, subdev, mask, callback, task);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return 0;
}

static RTAI_SYSCALL_MODE int _comedi_command(void *dev, comedi_cmd *cmdin)
{
	int i;
	comedi_cmd cmd = *cmdin;
	RTAI_COMEDI_LOCK(dev, cmd.subdev);
	rt_printk("COMEDI COMMAND: subdev = %u, flgas = %u, start_src = %u, start_arg = %u, scan_begin_src = %u, scan_begin_arg = %u, convert_src = %u, convert_arg = %u, scan_end_src = %u, scan_end_arg = %u, stop_src = %u, stop_arg = %u, chanlist_len = %u.\n", cmd.subdev, cmd.flags, cmd.start_src, cmd.start_arg, cmd.scan_begin_src, cmd.scan_begin_arg, cmd.convert_src, cmd.convert_arg, cmd.scan_end_src, cmd.scan_end_arg, cmd.stop_src, cmd.stop_arg, cmd.chanlist_len);
	for (i = 0; i < cmd.chanlist_len; i++) {
		rt_printk("CHANLIST: # %d, val = %u.\n", i, cmd.chanlist[i]);
	}
	RTAI_COMEDI_UNLOCK(dev, cmd.subdev);
	return 0;
}

static RTAI_SYSCALL_MODE int _comedi_command_test(void *dev, comedi_cmd *cmdin)
{
	int i;
	comedi_cmd cmd = *cmdin;
	RTAI_COMEDI_LOCK(dev, cmd.subdev);
	rt_printk("COMEDI COMMAND TEST: subdev = %u, flgas = %u, start_src = %u, start_arg = %u, scan_begin_src = %u, scan_begin_arg = %u, convert_src = %u, convert_arg = %u, scan_end_src = %u, scan_end_arg = %u, stop_src = %u, stop_arg = %u, chanlist_len = %u.\n", cmd.subdev, cmd.flags, cmd.start_src, cmd.start_arg, cmd.scan_begin_src, cmd.scan_begin_arg, cmd.convert_src, cmd.convert_arg, cmd.scan_end_src, cmd.scan_end_arg, cmd.stop_src, cmd.stop_arg, cmd.chanlist_len);
	for (i = 0; i < cmd.chanlist_len; i++) {
		rt_printk("CHANLIST: # %d, val = %u.\n", i, cmd.chanlist[i]);
	}
	RTAI_COMEDI_UNLOCK(dev, cmd.subdev);
	return 0;
}

static RTAI_SYSCALL_MODE int RT_comedi_command(void *dev, struct cmds_ofstlens *ofstlens, long test)
{
	int retval;
	comedi_cmd cmd[2]; // 2 instances, a safe room for 64 getting into 32
	memcpy(cmd, (void *)ofstlens + ofstlens->cmd_ofst, ofstlens->cmd_len);
	cmd[0].chanlist     = (void *)ofstlens + ofstlens->chanlist_ofst;
	cmd[0].chanlist_len = ofstlens->chanlist_len;
	cmd[0].data         = (void *)ofstlens + ofstlens->data_ofst;
	cmd[0].data_len     = ofstlens->data_len;
	retval = test ? _comedi_command_test(dev, cmd) : _comedi_command(dev, cmd);
	return retval;
}

RTAI_SYSCALL_MODE long rt_comedi_command_data_read(void *dev, unsigned int subdev, long nchans, lsampl_t *data)
{
	int i;
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI COMMAND READ: dev = %p, subdev = %u, nchans = %u.\n", dev, subdev, nchans);
	for (i = 0; i < nchans; i++) {
		data[i] = 0xcacca + i;
		rt_printk("CHAN: # %ld, data = %x.\n", i, data[i]);
	}
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return nchans;
}

#define WAIT       0
#define WAITIF     1
#define WAITUNTIL  2

static inline int __rt_comedi_command_data_wread(void *dev, unsigned int subdev, long nchans, lsampl_t *data, RTIME until, unsigned int *cbmaskarg, int waitmode)
{
	unsigned int cbmask, mask;
	long retval, kspace;
	if ((kspace = KSPACE(cbmaskarg))) {
		mask = cbmaskarg[0];
	} else {
		rt_get_user(mask, cbmaskarg);
	}
	switch (waitmode) {
		case WAIT:
			retval = rt_comedi_wait(&cbmask);
			break;
		case WAITIF:
			retval = rt_comedi_wait_if(&cbmask);
			break;
		case WAITUNTIL:
			retval = _rt_comedi_wait_until(&cbmask, until);
			break;
		default: // useless, just to avoid compiler warnings
			return RTE_PERM;
	}
	if (!retval && (mask & cbmask)) {
		if (kspace) {
			cbmaskarg[0] = cbmask;
		} else {
			rt_put_user(cbmask, cbmaskarg);
		}
		return rt_comedi_command_data_read(dev, subdev, nchans, data);
	}
	return retval;
}

RTAI_SYSCALL_MODE long rt_comedi_command_data_wread(void *dev, unsigned int subdev, long nchans, lsampl_t *data, unsigned int *cbmask)
{
	rt_printk("COMEDI COMMAND WREAD: dev = %p, subdev = %u, nchans = %u, cbmask = %u.\n", dev, subdev, nchans, *cbmask);
	return __rt_comedi_command_data_wread(dev, subdev, nchans, data, (RTIME)0, cbmask, WAIT);
}

RTAI_SYSCALL_MODE long rt_comedi_command_data_wread_if(void *dev, unsigned int subdev, long nchans, lsampl_t *data, unsigned int *cbmask)
{
	rt_printk("COMEDI COMMAND WREAD IF: dev = %p, subdev = %u, nchans = %u, cbmask = %u.\n", dev, subdev, nchans, *cbmask);
	return __rt_comedi_command_data_wread(dev, subdev, nchans, data, (RTIME)0, cbmask, WAITIF);
}

RTAI_SYSCALL_MODE long _rt_comedi_command_data_wread_until(void *dev, unsigned int subdev, long nchans, lsampl_t *data, unsigned int *cbmask, RTIME until)
{
	rt_printk("COMEDI COMMAND WREAD UNTIL: dev = %p, subdev = %u, nchans = %u, cbmask = %u, until = %lld.\n", dev, subdev, nchans, *cbmask, until);
	return __rt_comedi_command_data_wread(dev, subdev, nchans, data, until, cbmask, WAITUNTIL);
}

RTAI_SYSCALL_MODE long _rt_comedi_command_data_wread_timed(void *dev, unsigned int subdev, long nchans, lsampl_t *data, unsigned int *cbmask, RTIME delay)
{
	rt_printk("COMEDI COMMAND WREAD TIMED: dev = %p, subdev = %u, nchans = %u, cbmask = %u, until = %lld.\n", dev, subdev, nchans, *cbmask, delay);
	return _rt_comedi_command_data_wread_until(dev, subdev, nchans, data, cbmask, rt_get_time() + delay);
}

RTAI_SYSCALL_MODE long RT_comedi_command_data_wread(void *dev, unsigned int subdev, long nchans, lsampl_t *data, unsigned int *cbmask, unsigned int datalen, RTIME time)
{
	switch (nchans & 0x3) {
		case 0:
			return rt_comedi_command_data_wread(dev, subdev, nchans >> 2, data, cbmask);
		case 1:
			return rt_comedi_command_data_wread_if(dev, subdev, nchans >> 2, data, cbmask);
		case 2:
			return _rt_comedi_command_data_wread_until(dev, subdev, nchans >> 2, data, cbmask, time);
		case 3:
			return _rt_comedi_command_data_wread_timed(dev, subdev, nchans >> 2, data, cbmask, time);
	}
	return 0;
}

static RTAI_SYSCALL_MODE int _comedi_data_write(void *dev, unsigned int subdev, unsigned int chan, unsigned int range, unsigned int aref, lsampl_t data)
{
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI DATA WRITE: dev = %p, subdev = %u, chan = %u, range = %u, aref = %x, data = %u.\n", dev, subdev, chan, range, aref, data);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return 1;
}

static RTAI_SYSCALL_MODE int _comedi_data_read(void *dev, unsigned int subdev, unsigned int chan, unsigned int range, unsigned int aref, lsampl_t *data)
{
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI DATA READ: dev = %p, subdev = %u, chan = %u, range = %u, aref = %x.\n", dev, subdev, chan, range, aref);
	data[0] = 0xabcdea;
	rt_printk("COMEDI DATA READ RETURNS %x.\n", data[0]);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return 1;
}

static RTAI_SYSCALL_MODE int _comedi_data_read_delayed(void *dev, unsigned int subdev, unsigned int chan, unsigned int range, unsigned int aref, lsampl_t *data, unsigned int nanosec)
{
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI DATA READ DELAYED: dev = %p, subdev = %u, chan = %u, range = %u, aref = %x, nanosec = %u.\n", dev, subdev, chan, range, aref, nanosec);
	rt_sleep(nano2count(nanosec));
	data[0] = 0xcaccae;
	rt_printk("COMEDI DATA READ DELAYED RETURNS %x.\n", data[0]);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return 1;
}

static RTAI_SYSCALL_MODE int _comedi_data_read_hint(void *dev, unsigned int subdev, unsigned int chan, unsigned int range, unsigned int aref)
{
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI DATA READ HINT: dev = %p, subdev = %u, chan = %u, range = %u, aref = %x.\n", dev, subdev, chan, range, aref);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return 1;
}

static RTAI_SYSCALL_MODE int _comedi_dio_config(void *dev, unsigned int subdev, unsigned int chan, unsigned int io)
{
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI DIO CONFIG: dev = %p, subdev = %u, chan = %u, io = %u.\n", dev, subdev, chan, io);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return 1;
}

static RTAI_SYSCALL_MODE int _comedi_dio_read(void *dev, unsigned int subdev, unsigned int chan, unsigned int *val)
{
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI DIO READ: dev = %p, subdev = %u, chan = %u.\n", dev, subdev, chan);
	val[0] = 0xFFFFFFFF;
	rt_printk("COMEDI DIO READ RETURNS %x.\n", val[0]);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return 1;
}

static RTAI_SYSCALL_MODE int _comedi_dio_write(void *dev, unsigned int subdev, unsigned int chan, unsigned int val)
{
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI DIO WRITE: dev = %p, subdev = %u, chan = %u, val = %x.\n", dev, subdev, chan, val);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return 1;
}

static RTAI_SYSCALL_MODE int _comedi_dio_bitfield(void *dev, unsigned int subdev, unsigned int write_mask, unsigned int *bits)
{
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI DIO BITFIELD: dev = %p, subdev = %u, write_mask = %x, bits = %x.\n", dev, subdev, write_mask, bits[0]);
	bits[0] |= 0xFFFFFFFF;
	rt_printk("COMEDI DIO BITFIELD RETURNS %x.\n", bits[0]);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return 1;
}

static RTAI_SYSCALL_MODE int _comedi_get_n_subdevices(void *dev)
{
	rt_printk("COMEDI GET N SUBDEVICES, dev = %x.\n", dev);
	return 0;
}

static RTAI_SYSCALL_MODE int _comedi_get_version_code(void *dev)
{
	rt_printk("COMEDI GET VERSION CODE: dev = %x.\n", dev);
	return COMEDI_VERS;
}

RTAI_SYSCALL_MODE char *rt_comedi_get_driver_name(void *dev, char *name)
{
	rt_printk("COMEDI GET DRIVER NAME: dev = %x.\n", dev);
	strncpy(name, DRIVER_NAME, COMEDI_NAMELEN);
	rt_printk("COMEDI GET DRIVER NAME RETURNS %s.\n", DRIVER_NAME);
	return name;
}

RTAI_SYSCALL_MODE char *rt_comedi_get_board_name(void *dev, char *name)
{
	rt_printk("COMEDI GET BOARD NAME: dev = %p.\n", dev);
	strncpy(name, BOARD_NAME, COMEDI_NAMELEN);
	rt_printk("COMEDI GET BOARD NAME RETURNS %s.\n", BOARD_NAME);
	return name;
}

static RTAI_SYSCALL_MODE int _comedi_get_subdevice_type(void *dev, unsigned int subdev)
{
	rt_printk("COMEDI GET SUBDEV TYPE: dev = %p, subdev = %u.\n", dev, subdev);
	rt_printk("COMEDI GET SUBDEV TYPE RETURNS %u\n", SUBDEV_TYPE);
	return SUBDEV_TYPE;
}

static RTAI_SYSCALL_MODE int _comedi_find_subdevice_by_type(void *dev, int type, unsigned int start_subdevice)
{
	rt_printk("COMEDI FIND SUBDEV BY TYPE: dev = %p, type = %d, start_subdevice = %u.\n", dev, type, start_subdevice);
	rt_printk("COMEDI FIND SUBDEV BY TYPE RETURNS %u.\n", SUBDEV);
	return SUBDEV;
}

static RTAI_SYSCALL_MODE int _comedi_get_n_channels(void *dev, unsigned int subdev)
{
	rt_printk("COMEDI GET N CHANNELS: dev = %p, subdev = %u.\n", dev, subdev);
	rt_printk("COMEDI GET N CHANNELS RETURNS %u\n", N_CHANNELS);
	return N_CHANNELS;
}

static RTAI_SYSCALL_MODE lsampl_t _comedi_get_maxdata(void *dev, unsigned int subdev, unsigned int chan)
{
	rt_printk("COMEDI GET MAXDATA: dev = %p, subdev = %u, chan = %u.\n", dev, subdev, chan);
	rt_printk("COMEDI GET MAXDATA RETURNS %u.\n", MAXDATA);
	return MAXDATA;
}

static RTAI_SYSCALL_MODE int _comedi_get_n_ranges(void *dev, unsigned int subdev, unsigned int chan)
{
	rt_printk("COMEDI GET N RANGES: dev = %p, subdev = %u, chan = %u.\n", dev, subdev, chan);
	rt_printk("COMEDI GET N RANGES RETURNS %u\n", N_RANGES);
	return N_RANGES;
}

static RTAI_SYSCALL_MODE int _comedi_do_insn(void *dev, comedi_insn *insnin)
{
	int retval;
	int i;
	comedi_insn insn = *insnin;
	rt_printk("COMEDI DO INST: insn = %u, n = %u, data = %u, subdev = %u, chanspec = %u.\n", insn.insn, insn.n, insn.data[0], insn.subdev, insn.chanspec);
	for (i = 0; i < insn.n; i++) {
		insn.data[i] = 0xcacca0 + i;
		rt_printk("# = %d, %data = %u.\n", i, insn.data[i]);
	}
	retval = insn.n;
	rt_printk("COMEDI DO INST RETURNS %d.\n", retval);
	return retval;
}

RTAI_SYSCALL_MODE int rt_comedi_do_insnlist(void *dev, comedi_insnlist *ilist)
{
	int i;
	for (i = 0; i < ilist->n_insns; i ++) {
		if ((ilist->insns[i].n = _comedi_do_insn(dev, &ilist->insns[i])) < 0) {
			break;
		}
	}
	return i;
}

RTAI_SYSCALL_MODE int RT_comedi_do_insnlist(void *dev, long n_insns, struct insns_ofstlens *ofstlens)
{
#define RT_INSN(offset) (*((unsigned int *)(insns + ofstlens->offset)))
#define RT_INSN_ADR(offset) ((void *)ofstlens + ofstlens->offset)
	int i;
	void *insns              = RT_INSN_ADR(insns_ofst);
	unsigned int *data_ofsts = RT_INSN_ADR(data_ofsts);
	lsampl_t *data           = RT_INSN_ADR(data_ofst);
	comedi_insn insn[2]; // 2 instances, a safe room for 64 getting into 32
	for (i = 0; i < n_insns; i ++) {
		memcpy(insn, insns, ofstlens->insn_len);
		insn[0].data     = data + data_ofsts[i];
		insn[0].subdev   = RT_INSN(subdev_ofst);
		insn[0].chanspec = RT_INSN(chanspec_ofst);
		if ((RT_INSN(n_ofst) = _comedi_do_insn(dev, insn)) < 0) {
			break;
		}
		insns += ofstlens->insn_len;
	}
	return i;
}

static inline int _rt_comedi_trigger(void *dev, unsigned int subdev)
{
	comedi_insn insn;
	lsampl_t data = 0;
	insn.insn   = INSN_INTTRIG;
	insn.subdev = subdev;
	insn.n      = 1;
	insn.data   = &data;
	return _comedi_do_insn(dev, &insn);
}

RTAI_SYSCALL_MODE int rt_comedi_trigger(void *dev, unsigned int subdev)
{
	rt_printk("COMEDI TRIGGER: dev = %p, subdev = %u.\n", dev, subdev);
	return _rt_comedi_trigger(dev, subdev);
}

static RTAI_SYSCALL_MODE int _comedi_poll(void *dev, unsigned int subdev)
{
	int retval;
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI POLL: dev = %p, subdev = %u.\n", dev, subdev);
	retval = 33;
	rt_printk("COMEDI POLL RETURNS %d.\n", retval);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return retval;
}

static RTAI_SYSCALL_MODE unsigned int _comedi_get_subdevice_flags(void *dev, unsigned int subdev)
{
	int retval;
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI GET SUBDEVICE FLAGS: dev = %p, subdev = %u.\n", dev, subdev);
	retval = 0x77777777;
	rt_printk("COMEDI GET SUBDEVICE FLAGS RETURNS %d.\n", retval);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return retval;
}

static RTAI_SYSCALL_MODE int _comedi_get_krange(void *dev, unsigned int subdev, unsigned int chan, unsigned int range, comedi_krange *krange)
{
	rt_printk("COMEDI GET KRANGE: dev = %p, subdev = %u, chan = %u, range = %u.\n", dev, subdev, chan, range);
	rt_printk("COMEDI GET KRANGE RETURNS %d, %d, %u.\n", RANGE_MIN, RANGE_MAX, RANGE_FLAGS);
	*krange = (comedi_krange){ RANGE_MIN, RANGE_MAX, RANGE_FLAGS};
	return 0;
}

static RTAI_SYSCALL_MODE int _comedi_get_buf_head_pos(void * dev, unsigned int subdev)
{
	int retval;
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI GET BUF HEAD POS: dev = %p, subdev = %u.\n", dev, subdev);
	retval = 123456789;
	rt_printk("COMEDI GET BUF HEAD POS RETURNS %d.\n", retval);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return retval;
}

static RTAI_SYSCALL_MODE int _comedi_set_user_int_count(void *dev, unsigned int subdev, unsigned int buf_user_count)
{
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI SET USER INT COUNT: dev = %p, subdev = %u, buf_user_count = %u.\n", dev, subdev, buf_user_count);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return 0;
}

static RTAI_SYSCALL_MODE int _comedi_map(void *dev, unsigned int subdev, void *ptr)
{
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI MAP: dev = %p, subdev = %u.\n", dev, subdev);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return 0;
}

static RTAI_SYSCALL_MODE int _comedi_unmap(void *dev, unsigned int subdev)
{
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI UNMAP: dev = %p, subdev = %u.\n", dev, subdev);
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return 0;
}

static inline int __rt_comedi_wait(RTIME until, unsigned int *cbmask, int waitmode)
{
	if (cbmask) {
		long retval;
		RT_TASK *task = _rt_whoami();
		switch (waitmode) {
			case WAIT:
				rt_printk("COMEDI WAIT WARNING: REACHED AND SKIPPED.\n");
				rt_task_suspend_timed(task, nano2count(100000));
				break;
			case WAITIF:
				rt_task_suspend_if(task);
				break;
			case WAITUNTIL:
				rt_task_suspend_until(task, until);
				break;
			default: // useless, just to avoid compiler warnings
				return RTE_PERM;
		}
		retval = 0;
		task->resumsg = CBMASK;
		if (KSPACE(cbmask)) {
			cbmask[0] = (unsigned int)task->resumsg;
		} else {
			rt_put_user((unsigned int)task->resumsg, cbmask);
		}
		task->resumsg = 0;
		return retval;
	}
	return RTE_PERM;
}

RTAI_SYSCALL_MODE long rt_comedi_wait(unsigned int *cbmask)
{
	rt_printk("COMEDI WAIT: cbmask = %x.\n", *cbmask);
	return __rt_comedi_wait((RTIME)0, cbmask, WAIT);
}

RTAI_SYSCALL_MODE long rt_comedi_wait_if(unsigned int *cbmask)
{
	rt_printk("COMEDI WAIT IF: cbmask = %x.\n", *cbmask);
	return __rt_comedi_wait((RTIME)0, cbmask, WAITIF);
}

RTAI_SYSCALL_MODE long _rt_comedi_wait_until(unsigned int *cbmask, RTIME until)
{
	rt_printk("COMEDI WAIT UNTIL: cbmask = %x, until = %lld.\n", *cbmask, until);
	return __rt_comedi_wait(until, cbmask, WAITUNTIL);
}

RTAI_SYSCALL_MODE long _rt_comedi_wait_timed(unsigned int *cbmask, RTIME delay)
{
	rt_printk("COMEDI WAIT TIMED: cbmask = %x, delay = %lld.\n", *cbmask, delay);
	return _rt_comedi_wait_until(cbmask, rt_get_time() + delay);
}

RTAI_SYSCALL_MODE long rt_comedi_command_data_write(void *dev, unsigned int subdev, long nchans, lsampl_t *data)
{
	int i;
	RTAI_COMEDI_LOCK(dev, subdev);
	rt_printk("COMEDI COMMAND WRITE: dev = %p, subdev = %u, nchans = %u.\n", dev, subdev, nchans);
	for (i = 0; i < nchans; i++) {
		rt_printk("CHAN = %d, data = %x.\n", i, data[i]);
	}
	RTAI_COMEDI_UNLOCK(dev, subdev);
	return nchans;
}

static struct rt_fun_entry rtai_comedi_fun[] = {
  [_KCOMEDI_OPEN]                  = { 0, _comedi_open }
 ,[_KCOMEDI_CLOSE]                 = { 0, _comedi_close }
 ,[_KCOMEDI_LOCK]                  = { 0, _comedi_lock }
 ,[_KCOMEDI_UNLOCK]                = { 0, _comedi_unlock }
 ,[_KCOMEDI_CANCEL]                = { 0, _comedi_cancel }
 ,[_KCOMEDI_REGISTER_CALLBACK]     = { 0, rt_comedi_register_callback }
 ,[_KCOMEDI_COMMAND]               = { 0, _comedi_command }
 ,[_KCOMEDI_COMMAND_TEST]          = { 0, _comedi_command_test }
 ,[_KCOMEDI_TRIGGER    ]           = { 0, rt_comedi_trigger }
 ,[_KCOMEDI_DATA_WRITE]            = { 0, _comedi_data_write}
 ,[_KCOMEDI_DATA_READ]             = { 0, _comedi_data_read }
 ,[_KCOMEDI_DATA_READ_DELAYED]     = { 1, _comedi_data_read_delayed } // type 1 for this test only
 ,[_KCOMEDI_DATA_READ_HINT]        = { 0, _comedi_data_read_hint }
 ,[_KCOMEDI_DIO_CONFIG]            = { 0, _comedi_dio_config }
 ,[_KCOMEDI_DIO_READ]              = { 0, _comedi_dio_read }
 ,[_KCOMEDI_DIO_WRITE]             = { 0, _comedi_dio_write }
 ,[_KCOMEDI_DIO_BITFIELD]          = { 0, _comedi_dio_bitfield }
 ,[_KCOMEDI_GET_N_SUBDEVICES]      = { 0, _comedi_get_n_subdevices }
 ,[_KCOMEDI_GET_VERSION_CODE]      = { 0, _comedi_get_version_code }
 ,[_KCOMEDI_GET_DRIVER_NAME]       = { 0, rt_comedi_get_driver_name }
 ,[_KCOMEDI_GET_BOARD_NAME]        = { 0, rt_comedi_get_board_name }
 ,[_KCOMEDI_GET_SUBDEVICE_TYPE]    = { 0, _comedi_get_subdevice_type }
 ,[_KCOMEDI_FIND_SUBDEVICE_TYPE]   = { 0, _comedi_find_subdevice_by_type }
 ,[_KCOMEDI_GET_N_CHANNELS]        = { 0, _comedi_get_n_channels }
 ,[_KCOMEDI_GET_MAXDATA]           = { 0, _comedi_get_maxdata }
 ,[_KCOMEDI_GET_N_RANGES]          = { 0, _comedi_get_n_ranges }
 ,[_KCOMEDI_DO_INSN]               = { 0, _comedi_do_insn }
 ,[_KCOMEDI_DO_INSN_LIST]          = { 0, rt_comedi_do_insnlist }
 ,[_KCOMEDI_POLL]                  = { 0, _comedi_poll }
 ,[_KCOMEDI_GET_SUBDEVICE_FLAGS]   = { 0, _comedi_get_subdevice_flags }
 ,[_KCOMEDI_GET_KRANGE]            = { 0, _comedi_get_krange }
 ,[_KCOMEDI_GET_BUF_HEAD_POS]      = { 0, _comedi_get_buf_head_pos }
 ,[_KCOMEDI_SET_USER_INT_COUNT]    = { 0, _comedi_set_user_int_count }
 ,[_KCOMEDI_MAP]                   = { 0, _comedi_map }
 ,[_KCOMEDI_UNMAP]                 = { 0, _comedi_unmap }
 ,[_KCOMEDI_WAIT]                  = { 1, rt_comedi_wait }
 ,[_KCOMEDI_WAIT_IF]               = { 0, rt_comedi_wait_if }
 ,[_KCOMEDI_WAIT_UNTIL]            = { 1, _rt_comedi_wait_until }
 ,[_KCOMEDI_WAIT_TIMED]            = { 1, _rt_comedi_wait_timed }
 ,[_KCOMEDI_COMD_DATA_READ]        = { 0, rt_comedi_command_data_read }
 ,[_KCOMEDI_COMD_DATA_WREAD]       = { 1, rt_comedi_command_data_wread }
 ,[_KCOMEDI_COMD_DATA_WREAD_IF]    = { 0, rt_comedi_command_data_wread_if }
 ,[_KCOMEDI_COMD_DATA_WREAD_UNTIL] = { 1, _rt_comedi_command_data_wread_until }
 ,[_KCOMEDI_COMD_DATA_WREAD_TIMED] = { 1, _rt_comedi_command_data_wread_timed }
 ,[_KCOMEDI_COMD_DATA_WRITE]       = { 0, rt_comedi_command_data_write }
 ,[_RT_KCOMEDI_COMMAND]            = { 0, RT_comedi_command }
 ,[_RT_KCOMEDI_DO_INSN_LIST]       = { 0, RT_comedi_do_insnlist }
 ,[_RT_KCOMEDI_COMD_DATA_WREAD]    = { 0, RT_comedi_command_data_wread }
};

int __rtai_comedi_init(void)
{
	if( set_rt_fun_ext_index(rtai_comedi_fun, FUN_COMEDI_LXRT_INDX) ) {
		printk("Recompile your module with a different index\n");
		return -EACCES;
	}
	return 0;
}

void __rtai_comedi_exit(void)
{
	reset_rt_fun_ext_index(rtai_comedi_fun, FUN_COMEDI_LXRT_INDX);
}

module_init(__rtai_comedi_init);
module_exit(__rtai_comedi_exit);

EXPORT_SYMBOL(rt_comedi_wait);
EXPORT_SYMBOL(rt_comedi_wait_if);
EXPORT_SYMBOL(_rt_comedi_wait_until);
EXPORT_SYMBOL(_rt_comedi_wait_timed);
EXPORT_SYMBOL(rt_comedi_get_driver_name);
EXPORT_SYMBOL(rt_comedi_get_board_name);
EXPORT_SYMBOL(rt_comedi_register_callback);
EXPORT_SYMBOL(rt_comedi_command_data_read);
EXPORT_SYMBOL(rt_comedi_command_data_wread);
EXPORT_SYMBOL(rt_comedi_command_data_wread_if);
EXPORT_SYMBOL(_rt_comedi_command_data_wread_until);
EXPORT_SYMBOL(_rt_comedi_command_data_wread_timed);
EXPORT_SYMBOL(rt_comedi_do_insnlist);
EXPORT_SYMBOL(rt_comedi_trigger);
EXPORT_SYMBOL(rt_comedi_command_data_write);
