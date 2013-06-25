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
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <signal.h>
#include <unistd.h>

#include <rtai_fifos.h>

static int end;

static void endme (int dummy) { end = 1; }

int main(int argc,char *argv[])
{
	int fd0;
	char nm[RTF_NAMELEN+1];
	struct sample { long min, max, avrg, jitters[2]; } samp;

	if ((fd0 = open(rtf_getfifobyminor(0,nm,sizeof(nm)), O_RDONLY)) < 0) {
		fprintf(stderr, "Error opening %s\n",nm);
		exit(1);
	}

	signal (SIGINT, endme);

	while (!end) {
		read(fd0, &samp, sizeof(samp));
		printf("* latency: min: %ld, max: %ld, average: %ld; fastjit: %ld, slowjit: %ld * (all us) *\n", samp.min/1000, samp.max/1000, samp.avrg/1000, samp.jitters[0]/1000, samp.jitters[1]/1000);
		fflush(stdout);
	}
	return 0;
}
