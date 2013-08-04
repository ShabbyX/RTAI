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
#include <unistd.h>
#include <sys/types.h>

#include <rtai_sem.h>

#include "clock.h"

static enum {stopped, running} Clockstatus;
static char buffer;
static SEM *notEmpty, *notFull;

void CommandClock_Put(char command)
{
	rt_sem_wait(notFull);
	if (((Clockstatus == running) == (command == 'T')) || command == 'F') {
		buffer = command;
		rt_sem_signal(notEmpty);
	} else {
		rt_sem_signal(notFull);
	}
}

void CommandClock_Get(char *command)
{
	switch (Clockstatus) {
		case stopped:
			rt_sem_wait(notEmpty);
			*command = buffer;
			if (*command == 'R') {
				Clockstatus = running;
			}
			rt_sem_signal(notFull);
			break;
		case running:
			if (rt_sem_wait_if(notEmpty) > 0) {
				*command = buffer;
				if (*command == 'T') {
					Clockstatus = stopped;
				}
				rt_sem_signal(notFull);
			} else {
				*command = 'R';
			}
			break;
	}
}

int init_cmdclk(void)
{
	notEmpty = rt_sem_init(nam2num("NOTECL"), 0);
	notFull = rt_sem_init(nam2num("NOTFCL"), 1);
	Clockstatus = stopped;
	return 0;
}

int cleanup_cmdclk(void)
{
	rt_sem_delete(notEmpty);
	rt_sem_delete(notFull);
	return 0;
}
