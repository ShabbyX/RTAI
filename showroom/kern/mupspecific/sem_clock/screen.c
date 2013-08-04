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


/* SCREEN */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <rtai_fifos.h>

int main(void)
{
	unsigned int Keyboard, Screen, i;
	char d = 'd';
	char chain[12];
	char displine[40] = "CLOCK-> 00:00:00  CHRONO-> 00:00:00";

	if ((Keyboard = open("/dev/rtf0", O_WRONLY)) < 0) {
		fprintf(stderr, "Error opening /dev/rtf0 in screen\n");
		exit(1);
	}

	if (write(Keyboard, &d, 1) < 0 ) {
		fprintf(stderr, "Can't send initial command to RT-task\n");
		exit(1);
	}

	close(Keyboard);

	if ((Screen = open("/dev/rtf1", O_RDONLY)) < 0) {
		fprintf(stderr, "Error opening /dev/rtf1\n");
		exit(1);
	}

	while(1) {
		read(Screen, chain, 12);
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
	return 0;
}
