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


/* MODULE CHRONO */

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
#include <rtai_fifos.h>

#include "clock.h"

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
			if (rt_sem_wait_timed(&notEmpty, fiveSeconds) >= 0) {
				buffered = TRUE;
			}
			*command = 'R';
			Chronostatus = stoppedInitial;
			break;
	}
}

int init_module(void)
{
	rt_sem_init(&notEmpty, 0);
	rt_sem_init(&notFull, 1);
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
