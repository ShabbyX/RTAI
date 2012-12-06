/*

COPYRIGHT (C) 1999  Paolo Mantegazza (mantegazza@aero.polimi.it)

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

#include <semaphore.h>

#include "clock.h"

static enum {stopped, running} Clockstatus;
static char buffer;
static sem_t notEmpty, notFull;

void CommandClock_Put(char command)
{
	sem_wait(&notFull);
	if (((Clockstatus == running) == (command == 'T')) || command == 'F') {
		buffer = command;
		sem_post(&notEmpty);
	} else {
		sem_post(&notFull);
	}
}

void CommandClock_Get(char *command)
{
	switch (Clockstatus) {
		case stopped:
			sem_wait(&notEmpty);
			*command = buffer;
			if (*command == 'R') {
				Clockstatus = running;
			}
			sem_post(&notFull);
			break;
		case running:
			if (!sem_trywait(&notEmpty)) {
				*command = buffer;
				if (*command == 'T') {
					Clockstatus = stopped;
				}
				sem_post(&notFull);
			} else {
				*command = 'R';
			}
			break;
	}
}

int init_cmdclk(void)
{
	sem_init(&notEmpty, 0, 0);
	sem_init(&notFull, 0, 1);
	Clockstatus = stopped;
	return 0;
}

int cleanup_cmdclk(void)
{
	sem_destroy(&notEmpty);
	sem_destroy(&notFull);
	return 0;
}
