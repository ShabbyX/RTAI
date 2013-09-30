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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <rtai_msg.h>

#include "clock.h"

static enum {stopped, running} Clockstatus;

void CommandClock_Put(char command)
{
	static RT_TASK *ackn = 0;
	unsigned int put = 'a';
	unsigned long msg;

	if (ackn != rt_get_adr(nam2num("CLKTSK"))) {
		ackn = rt_rpc(rt_get_adr(nam2num("CLKTSK")), put, &msg);
	}
	if (((Clockstatus == running) == (command == 'T')) || command == 'F') {
		rt_send(rt_get_adr(nam2num("CLKTSK")), (unsigned int)command);
	}
}

void CommandClock_Get(char *command)
{
	static RT_TASK *ackn = 0;
	unsigned int get = 'b';
	unsigned long msg;

	if (ackn != rt_get_adr(nam2num("CLKTSK"))) {
		ackn = rt_rpc(rt_get_adr(nam2num("CLKTSK")), get, &msg);
	}
	rt_receive(rt_get_adr(nam2num("CLKTSK")), &msg);
	*command = (char)msg;
}

void *CommandClock_task(void *args)
{
	RT_TASK *mytask;
	unsigned long command;
	char R = 'R';
	int ackn = 0;
	RT_TASK *get = (RT_TASK *)0, *put = (RT_TASK *)0, *task;

 	if (!(mytask = rt_thread_init(nam2num("CLKTSK"), 1, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT TASK CommandClock_task\n");
		exit(1);
	}
	printf("INIT TASK CommandClock_task %p.\n", mytask);
	mlockall(MCL_CURRENT | MCL_FUTURE);

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
		if (command == 'F') {
			goto end;
		}
	}
end:
	rt_task_delete(mytask);
	printf("END TASK CommandClock_task %p.\n", mytask);
	return 0;
}
