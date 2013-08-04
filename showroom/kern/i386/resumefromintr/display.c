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
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <sys/poll.h>

static int end;

static void endme(int dummy) { end = 1; }

int main(void)
{
	int maxj, cmd, count = 0;
	struct pollfd kbrd = { 0, POLLIN };
#ifdef unused
	struct sched_param mysched;
	mysched.sched_priority = 99;
	if( sched_setscheduler( 0, SCHED_FIFO, &mysched ) == -1 ) {
	puts(" ERROR IN SETTING THE SCHEDULER UP");
	perror( "errno" );
	exit( 0 );
 	}
#endif
	signal(SIGINT, endme);
	if ((cmd = open("/dev/rtf0", O_RDONLY)) < 0) {
		fprintf(stderr, "Error opening /dev/rtf0\n");
		exit(1);
	}
	while(!end) {
		if (poll(&kbrd, 1, 0)) {
			break;
		}
		read(cmd, &maxj, sizeof(int));
		printf("> count = %d, maxj = %d (us).\n", count, maxj);
		count++;
	}
	return 0;
}
