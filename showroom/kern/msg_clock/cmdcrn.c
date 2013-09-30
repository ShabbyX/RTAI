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

#include <asm/io.h>

#include <asm/rtai.h>
#include <rtai_sched.h>
#include <rtai_sem.h>
#include <rtai_msg.h>

#include "clock.h"

MODULE_LICENSE("GPL");

#define FIVE_SECONDS 5000000000LL
static enum {stoppedInitial, running, stoppedFinal} Chronostatus;
static RT_TASK Chrono;

void CommandChrono_Put(char command)
{
	static RT_TASK *ackn = 0;
	unsigned int put = 'c';
	unsigned long msg;

	if (ackn != &Chrono) {
		ackn = rt_rpc(&Chrono, put, &msg);
	}
	if ((Chronostatus == running) != (command == 'C')) {
		rt_send(&Chrono, (unsigned int)command);
	}
}

void CommandChrono_Get(char *command)
{
	static RT_TASK *ackn = 0;
	unsigned int get = 'd';
	unsigned long msg;

	if (ackn != &Chrono) {
		ackn = rt_rpc(&Chrono, get, &msg);
	}
	rt_receive(&Chrono, &msg);
	*command = (char)msg;
}

extern int cpu_used[];

static void CommandChrono_task(long t)
{
	RTIME fiveSeconds = nano2count(FIVE_SECONDS);
	unsigned long command;
	unsigned int buffered = 0;
	unsigned int C = 'C';
	unsigned int R = 'R';
	int ackn = 0;
	RT_TASK *get = (RT_TASK *)0, *put = (RT_TASK *)0, *task;

	Chronostatus = stoppedInitial;
	while (ackn != ('c' + 'd')) {
		task = rt_receive((RT_TASK *)0, &command);
		switch (command) {
			case 'd':
				get = task;
				ackn += command;
				break;
			case 'c':
				put = task;
				ackn += command;
				break;
		}
	}
	rt_return(put, command);
	rt_return(get, command);

	while(1) {
		cpu_used[hard_cpu_id()]++;
		switch (Chronostatus) {
			case stoppedInitial:
				if (buffered) {
					command = buffered;
					buffered = 0;
				} else {
					rt_receive(put, &command);
				}
				Chronostatus = running;
				break;
			case running:
				if (rt_receive_if(put, &command)) {
					if (command == 'E') {
						Chronostatus = stoppedFinal;
					}
				} else {
					command = C;
				}
				break;
			case stoppedFinal:
				Chronostatus = stoppedInitial;
				if (rt_receive_timed(put, &command, fiveSeconds) > 0) {
					buffered = command;
				}
				command = R;
				break;
		}
		rt_send(get, command);
	}
}

int init_module(void)
{
	rt_task_init(&Chrono, &CommandChrono_task, 0, 2000, 0, 0, 0);
	rt_task_resume(&Chrono);
	return 0;
}

void cleanup_module(void)
{
	rt_task_delete(&Chrono);
}

EXPORT_SYMBOL(CommandChrono_Put);
EXPORT_SYMBOL(CommandChrono_Get);
