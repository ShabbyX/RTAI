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


/* KEYBOARD */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <termio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <rtai_fifos.h>

static char get_key(void)
{
	static struct termio my_kb, orig_kb;
	static int first = 1;
	char ch;
	if (first) {
		first = 0;
		ioctl(0, TCGETA, &orig_kb);
		my_kb = orig_kb;
		my_kb.c_lflag &= ~(ECHO | ISIG | ICANON);
		my_kb.c_cc[4] = 1;
	}
	ioctl(0, TCSETAF, &my_kb);
	ch = getchar();
	ioctl(0, TCSETAF, &orig_kb);
	return ch;
}

static void menu(void)
{
	printf("___________________________________________________\n");
	printf("|                CLOCK COMMANDS                   |\n");
	printf("|t-stop clock, r-run, h-hour, m-minutes, s-seconds|\n");
	printf("|              CHRONOMETER COMMANDS               |\n");
	printf("|    c-run, i-display intermediate time, e-stop   |\n");
	printf("|               CONTROL COMMANDS                  |\n");
	printf("| n-show/hide display, p-display menu f-end this  |\n");
	printf("---------------------------------------------------\n");
	printf("\n");
}

int main(void)
{
	int Keyboard, pid;
	char ch;
	char k = 'k';
	
	menu();
	pid = fork();
	if (!pid) {
		execl("./screen", "./screen", NULL);
	}
	sleep(1);
	
	if ((Keyboard = open("/dev/rtf0", O_WRONLY)) < 0) {
		fprintf(stderr, "Error opening /dev/rtf0 in keybrd\n");
		exit(1);
	}

	if (write(Keyboard, &k, 1) < 0 ) {
		fprintf(stderr, "Can't send initial command to RT-task\n");
		exit(1);
	}

	do {
	        ch = get_key();
	        if (ch == 'p' || ch == 'P') {
			menu();
		}
		if (write(Keyboard, &ch, 1) < 0 ) {
			fprintf(stderr, "Can't send command to RT-task\n");
			exit(1);
		}
	} while(ch != 'f');
	kill(pid, SIGKILL);
	exit(0);
}
