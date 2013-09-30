/*
 * Copyright (C) 2007 Paolo Mantegazza <mantegazza@aero.polimi.it>
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

/*
 * illustrating the two modes available to use
 * a task switch signal from kernel space.
 */

#include <linux/kernel.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");

#include "rtai_signal.h"

#define PERIOD  100000
#define STKSZ   4000
#define SWITCH_SIGNAL  1

static RT_TASK rtai_sig_task, sig_task;

static void tsk_sighdl(long signal, RT_TASK *task)
{
	static unsigned long cnt = 0, rpt = 1000000000/PERIOD;
	if (++cnt > rpt) {
		rt_printk("TSK SIGHDL > # sw: %lu, tsk: %p, sig: %lu.\n", rpt, task, signal);
		rpt += 1000000000/PERIOD;
	}
}

static void sighdl(void)
{
	static unsigned long cnt = 0, rpt = 1000000000/PERIOD;
	if (++cnt > rpt) {
		rt_printk("FUN SIGHDL > # sw: %lu, tsk: %p.\n", rpt, rt_whoami());
		rpt += 1000000000/PERIOD;
	}
}

static void tsk_sig_fun(long taskidx)
{
	rt_request_signal(SWITCH_SIGNAL, tsk_sighdl);
	rt_task_signal_handler(&rtai_sig_task, (void *)SWITCH_SIGNAL);
	while (1) {
		rt_sleep(nano2count(PERIOD));
	}
}

static void sig_fun(long arg)
{
/*
 * it could be here, but it has been done at task init already, so it
 * might be useful here just for changes, including nullifing it.
 *	rt_task_signal_handler(&sig_task, sighdl);
 */
	while (1) {
		rt_sleep(nano2count(PERIOD));
	}
}

int init_module(void)
{
	start_rt_timer(0);
	rt_task_init(&rtai_sig_task, tsk_sig_fun, 0, STKSZ, 0, 0, 0);
	rt_task_resume(&rtai_sig_task);
	rt_task_init(&sig_task, sig_fun, 0, STKSZ, 0, 0, sighdl);
	rt_task_resume(&sig_task);
	return 0;
}

void cleanup_module(void)
{
	stop_rt_timer();
	rt_task_delete(&rtai_sig_task);
	rt_task_delete(&sig_task);
}
