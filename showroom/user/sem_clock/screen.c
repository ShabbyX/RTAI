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

#include <rtai_mbx.h>

static void endme(int dummy)
{
	rt_task_delete(rt_get_adr(nam2num("SCRTSK")));
	signal(SIGINT, SIG_DFL);
	exit(1);
}

int main(void)
{
	RT_TASK *mytask;
	MBX *Keyboard, *Screen;
	unsigned int i;
	char d = 'd';
	char chain[12];
	char displine[40] = "CLOCK-> 00:00:00  CHRONO-> 00:00:00";

	signal(SIGINT, endme);
 	if (!(mytask = rt_task_init(nam2num("SCRTSK"), 20, 0, 0))) {
		printf("CANNOT INIT SCREEN TASK\n");
		exit(1);
	}

 	if (!(Keyboard = rt_get_adr(nam2num("KEYBRD")))) {
		printf("CANNOT FIND KEYBOARD MAILBOX\n");
		exit(1);
	}

	if (rt_mbx_send(Keyboard, &d, 1) > 0 ) {
		fprintf(stderr, "Can't send initial command to RT-task\n");
		exit(1);
	}

 	if (!(Screen = rt_get_adr(nam2num("SCREEN")))) {
		printf("CANNOT FIND SCREEN MAILBOX\n");
		exit(1);
	}

	while(1) {
		rt_mbx_receive(Screen, chain, 12);
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
	rt_task_delete(mytask);
}
