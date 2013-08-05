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


/* MODULE CHRONO */

#include <linux/kernel.h>
#include <linux/module.h>

#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_sem.h>

#include "clock.h"

MODULE_LICENSE("GPL");

#define FIVE_SECONDS 5000000000LL

static RTIME fiveSeconds;
static enum {stoppedInitial, running, stoppedFinal} Chronostatus;
static char buffer;
static SEM notEmpty, notFull;
static BOOLEAN buffered;

void CommandChrono_Put(char command)
{
	rt_sem_wait(&notFull);
	if ((Chronostatus == running) != (command == 'C')) {
		buffer = command;
		rt_sem_signal(&notEmpty);
	} else {
		rt_sem_signal(&notFull);
	}
}

void CommandChrono_Get(char *command)
{
	switch (Chronostatus) {
		case stoppedInitial:
			if (buffered) {
				buffered = FALSE;
			} else {
				rt_sem_wait(&notEmpty);
			}
			*command  = buffer;
			Chronostatus = running;
			rt_sem_signal(&notFull);
			break;
		case running:
			if (rt_sem_wait_if(&notEmpty) > 0) {
				*command = buffer;
	 			if (*command == 'E') {
					Chronostatus = stoppedFinal;
				}
				rt_sem_signal(&notFull);
			} else {
				*command = 'C';
			}
			break;
		case stoppedFinal:
			if (rt_sem_wait_timed(&notEmpty, fiveSeconds) != SEM_TIMOUT) {
				buffered = TRUE;
			}
			*command = 'R';
			Chronostatus = stoppedInitial;
			break;
	}
}

int init_module(void)
{
	rt_typed_sem_init(&notEmpty, 0, SEM_TYPE);
	rt_typed_sem_init(&notFull, 1, SEM_TYPE);
	Chronostatus = stoppedInitial;
	buffered = FALSE;
	fiveSeconds = nano2count(FIVE_SECONDS);
	return 0;
}

void cleanup_module(void)
{
}

EXPORT_SYMBOL(CommandChrono_Put);
EXPORT_SYMBOL(CommandChrono_Get);
