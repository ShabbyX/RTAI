/*
 * COPYRIGHT (C) 2001  Paolo Mantegazza <mantegazza@aero.polimi.it>
 *               2002  Robert Schwebel  <robert@schwebel.de>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <rtai_fifos.h>

static int end;

static void endme(int dummy) { end = 1;}

int main(int argc, char *argv[])
{
	int fd0;
	char line[256];
	char nm[RTF_NAMELEN+1];
	FILE *procfile;
	long long max = -1000000000, min = 1000000000;
	struct sample { long long min; long long max; int index, ovrn; } samp;
	int n = 0, period, avrgtime;

	signal(SIGINT, endme);

	if ((fd0 = open(rtf_getfifobyminor(3,nm,sizeof(nm)), O_RDONLY)) < 0) {
		fprintf(stderr, "Error opening %s\n",nm);
		exit(1);
	}
	read(fd0, &period, sizeof(period));
	read(fd0, &avrgtime, sizeof(avrgtime));

	printf("RTAI Testsuite - KERNEL threads latency (all data in nanoseconds)\n");
        printf("\n*** latency verification tool with kernel threads ***\n");
        printf("***    period = %i (ns),  avrgtime = %i (s)    ***\n\n", period, avrgtime);

	while (!end) {
		if ((n++ % 21)==0) {
			printf("RTH|%11s|%11s|%11s|%11s|%11s|%11s\n", "lat min", "ovl min", "lat avg", "lat max", "ovl max", "overruns");
		}
		read(fd0, &samp, sizeof(samp));
		if (min > samp.min) min = samp.min;
		if (max < samp.max) max = samp.max;
		printf("RTD|%11lld|%11lld|%11d|%11lld|%11lld|%11d\n", samp.min, min, samp.index, samp.max, max, samp.ovrn);
		fflush(stdout);
	}

	close(fd0);

	return 0;
}
