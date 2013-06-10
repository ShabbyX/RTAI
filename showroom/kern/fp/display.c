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
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>

#define LIM 1234567890.

static int end;

static void endme(int dummy) { end = 1;}

int main(int argc,char *argv[])
{
	char ch;
	double s, a, sine[3];
	int rtf, cmd, flag = 1;

	signal(SIGINT, endme);

	if ((rtf = open("/dev/rtf0", O_RDONLY)) < 0) {
		fprintf(stderr, "Error opening /dev/rtf0\n");
		exit(1);
	}
	if ((cmd = open("/dev/rtf1", O_WRONLY)) < 0) {
		fprintf(stderr, "Error opening /dev/rtf1\n");
		exit(1);
	}
	write(cmd, &flag, 4);
	printf("\n***** CHECK FLOATING POINT SUPPORT *****\n");
	printf("***** PRESS f TO TOGGLE FLOATING POINT SUPPORT AND SEE WHAT HAPPENS *****\n\n");
	printf("***** PRESS ANY OTHER KEY TO KEEP VERIFYING THE CURRENT STATUS *****\n");
	for (s = a = 0.; s < LIM; a += 1., s += a);
	printf("***** THE RIGHT MOST VALUE SHOULD BE%19.10E *****\n\n", s);
	while(!end) {
		for (s = a = 0.; s < LIM; a += 1., s += a);
		for (s = a = 0.; s < LIM; a += 1., s += a);
		read(rtf, sine, 16);
		sine[2] = sin(sine[0]);
		printf("%25.16E%25.16E%19.10E\n", sine[1], sine[2], s);
		ch = getchar();
		if (ch == 'f') {
			flag = 1 - flag;
			write(cmd, &flag, 4);
		}
	}

	return 0;
}
