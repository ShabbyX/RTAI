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
#include <rtai_posix.h>

#include "clock.h"

#define FIVE_SECONDS 5

static struct timespec fiveSeconds;
static enum {stoppedInitial, running, stoppedFinal} Chronostatus;
static char buffer;
static sem_t notEmpty, notFull;
static BOOLEAN buffered;

void CommandChrono_Put(char command)
{
	sem_wait(&notFull);
	if (((Chronostatus == running) != (command == 'C')) || command == 'F') {
		buffer = command;
		sem_post(&notEmpty);
	} else {
		sem_post(&notFull);
	}
}

void CommandChrono_Get(char *command)
{
	switch (Chronostatus) {
		case stoppedInitial:
			if (buffered) {
				buffered = FALSE;
			} else {
				sem_wait(&notEmpty);
			}
			*command  = buffer;
			Chronostatus = running;
			sem_post(&notFull);
			break;
		case running:
			if (!sem_trywait(&notEmpty)) {
				*command = buffer;
	 			if (*command == 'E') {
					Chronostatus = stoppedFinal;
				}
				sem_post(&notFull);
			} else {
				*command = 'C';
			}
			break;
		case stoppedFinal:
			clock_gettime(CLOCK_MONOTONIC, &fiveSeconds);
			fiveSeconds.tv_sec += FIVE_SECONDS;
			if (!sem_timedwait(&notEmpty, &fiveSeconds)) {
				buffered = TRUE;
			}
			*command = 'R';
			Chronostatus = stoppedInitial;
			break;
	}
}

int init_cmdcrn(void)
{
	sem_init(&notEmpty, 0, 0);
	sem_init(&notFull, 0, 1);
	Chronostatus = stoppedInitial;
	buffered = FALSE;
	return 0;
}

int cleanup_cmdcrn(void)
{
	sem_destroy(&notEmpty);
	sem_destroy(&notFull);
	return 0;
}
