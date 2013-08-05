/*
COPYRIGHT (C) 1999  Paolo Mantegazza <mantegazza@aero.polimi.it>

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


/* MODULE CLOCK */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <asm/system.h>
#include <asm/fixmap.h>
#include <asm/smp.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/atomic.h>
#include <asm/desc.h>

#include <asm/rtai.h>
#include <rtai_sched.h>
#include <rtai_sem.h>
#include <rtai_msg.h>
#include <rtai_fifos.h>

#include "clock.h"

static enum {stopped, running} Clockstatus;
static RT_TASK Clock;

void CommandClock_Put(char command)
{
	static RT_TASK *ackn = 0;
	unsigned int put = 'a';
	unsigned long msg;

	if (ackn != &Clock) {
		ackn = rt_rpc(&Clock, put, &msg);
	}
	if ((Clockstatus == running) == (command == 'T')) {
		rt_send(&Clock, (unsigned int)command);
	}
}

void CommandClock_Get(char *command)
{
	static RT_TASK *ackn = 0;
	unsigned int get = 'b';
	unsigned long msg;

	if (ackn != &Clock) {
		ackn = rt_rpc(&Clock, get, &msg);
	}
	rt_receive(&Clock, &msg);
	*command = (char)msg;
}

extern int cpu_used[];

static void CommandClock_task(long t)
{
	unsigned long command;
	char R = 'R';
	int ackn = 0;
	RT_TASK *get = (RT_TASK *)0, *put = (RT_TASK *)0, *task;

	Clockstatus = stopped;
	while (ackn != ('a' + 'b')) {
		task = rt_receive((RT_TASK *)0, &command);
		switch (command) {
			case 'b':
				get = task;
				ackn += command;
				break;
			case 'a':
				put = task;
				ackn += command;
				break;
		}
	}
	rt_return(put, command);
	rt_return(get, command);

	while(1) {
		cpu_used[hard_cpu_id()]++;
		switch (Clockstatus) {
			case stopped:
				rt_receive(put, &command);
				if (command == 'R') {
					Clockstatus = running;
				}
				break;
			case running:
				if (rt_receive_if(put, &command)) {
					if (command == 'T') {
						Clockstatus = stopped;
					}
				} else {
					command = R;
				}
				break;
		}
		rt_send(get, command);
	}
}

int init_module(void)
{
	rt_task_init_cpuid(&Clock, CommandClock_task, 0, 2000, 0, 0, 0, 0);
	rt_task_resume(&Clock);
	return 0;
}

void cleanup_module(void)
{
	rt_task_delete(&Clock);
}

EXPORT_SYMBOL(CommandClock_Put);
EXPORT_SYMBOL(CommandClock_Get);
