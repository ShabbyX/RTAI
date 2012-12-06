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


/* SCREEN */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>

#include <rtai_pmq.h>

static volatile int end;
static RT_TASK *mytask;

static void endme(int dummy)
{
	end = 1;
}

int main(void)
{
	unsigned int i;
	char d = 'd';
	char chain[12];
	char displine[40] = "CLOCK-> 00:00:00  CHRONO-> 00:00:00";
	mqd_t Keyboard, Screen;
	struct mq_attr kb_attrs = { MAX_MSGS, 1, 0, 0};
	struct mq_attr sc_attrs = { MAX_MSGS, 12, 0, 0};
	
	signal(SIGINT, endme);
 	if (!(mytask = rt_task_init(nam2num("SCRTSK"), 20, 0, 0))) {
		printf("CANNOT INIT SCREEN TASK\n");
		exit(1);
	}

	if ((Keyboard = mq_open("KEYBRD", O_WRONLY | O_NONBLOCK, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, &kb_attrs)) <= 0) {
		printf("CANNOT FIND KEYBOARD MAILBOX\n");
		exit(1);
	}
	if (mq_send(Keyboard, &d, 1, 0) <= 0) {
		fprintf(stderr, "Screen can't send initial command to RT-task\n");
		exit(1);
	}
	mq_close(Keyboard);

	if ((Screen = mq_open("SCREEN", O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, &sc_attrs)) <= 0) {
		printf("CANNOT FIND SCREEN MAILBOX\n");
		exit(1);
	}

	while (!end) {
		if (mq_receive(Screen, chain, 12, NULL) <= 0) {
			continue;
		}
		if (end) {
			break;
		}
		if (chain[0] == 't') {
			for (i = 0; i < 11; i++) {
				displine[i+27] = chain[i+1];
			}
		} else if (chain[0] == 'h') {
			for (i = 0; i < 8; i++) {
				displine[i+8] = chain[i+1];
			}
		}
		printf("%s\n", displine);
	}
	mq_close(Screen);
	rt_task_delete(mytask);
	return 0;
}
