/*
COPYRIGHT (C) 2003  Paolo Mantegazza <mantegazza@aero.polimi.it>

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
#include <string.h>
#include <errno.h>
#include <termio.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>

#include <asm/rtai_srq.h>
#include "cal.h"

static char getkey(void)
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

#define getint(val) { \
	int tmp;					\
	fgets(str, sizeof(str), stdin);			\
	str[strlen(str) - 1] = 0;			\
	if (!str[0] || (tmp = atoi(str)) <= 0) {	\
		printf("*** INVALID ***\n");		\
		break;					\
	}						\
	val = tmp;					\
}

static void endme(int dummy) { }

int main(void)
{
	int srq, fifo, command;
	struct params_t params;
	char str[80];
	unsigned long args[MAXARGS];
	struct pollfd polls[2] = { { 0, POLLIN }, { 0, POLLIN } };

	signal(SIGINT, endme);
	srq = rtai_open_srq(CALSRQ);
	if ((fifo = open("/dev/rtf0", O_RDONLY)) < 0) {
		printf("Error opening FIF0\n");
		exit(1);
	}
	polls[1].fd = fifo;
	read(fifo, &params, sizeof(params));

	while (1) {
		printf("\n< IF YOU ARE USING THE RTAI_LXRT SCHEDULER NOTICE THAT RTAI OWN KERNEL KTASKS >\n<    ARE LINUX KTHREADS, SO YOU'LL CALIBRATE HARDENED LINUX KTHREADS ONLY     >\n");
		printf("\n\n- 8254 programming time.\n");
		printf("- Kernel space latency, rtai own kernel tasks.\n");
		printf("- kernel space latency, hardened linuX kthreads.\n");
		printf("- User space latency.\n");
		printf("- Cpu frequency calibration.\n");
		printf("- Apic frequency calibration.\n");
		printf("- Both cpu and apic frequency calibration.\n");
		printf("- bus Lock check.\n");
		printf("- End.\n");
		printf("< For a choice: keypress 8 or the only Upper case character in each line. >\n");
		str[0] = getkey();
		switch(command = tolower(str[0])) {

			case '8': {
				args[0] = CAL_8254;
			 	printf("\n*** '#define SETUP_TIME_8254 %lu', IN USE %lu ***\n\n", (unsigned long) rtai_srq(srq, (unsigned long)args), params.setup_time_8254);
				break;
			}

			case 'k':
			case 'x': {
				int average;
				args[0] = command == 'k' ? KLATENCY : KTHREADS;
				printf("Calibration period (microseconds): ");
				getint(args[1]);
				printf("Calibration time (seconds): ");
				getint(args[2]);
				printf("\n*** Wait %lu seconds for it ... ***\n", args[2]);
				args[2] = (1000000*args[2])/args[1];
				args[1] *= 1000;
				rtai_srq(srq, (unsigned long)args);
				read(fifo, &average, sizeof(average));
				average /= (int)args[2];
				if (params.mp) {
					printf("\n*** '#define LATENCY_APIC %d' (IN USE %lu)\n\n", (int)params.latency_apic + average, params.latency_apic);
				} else {
					printf("\n*** '#define LATENCY_8254 %d' (IN USE %lu)\n\n", (int)params.latency_8254 + average, params.latency_8254);
				}
				args[0] = END_KLATENCY;
				rtai_srq(srq, (unsigned long)args);
				break;
			}

			case 'u': {
				int pid, average;
				printf("Calibration period (microseconds): ");
				getint(args[1]);
				printf("Calibration time (seconds): ");
				getint(args[2]);
				printf("\n*** Wait %lu seconds for it ... ***\n", args[2]);
				args[2] = (1000000*args[2])/args[1];
				args[1] *= 1000;
				sprintf(str, "%lu", args[1]);
				sprintf(str + sizeof(str)/2, "%lu", args[2]);
				pid = fork();
				if (!pid) {
					execl("./ucal", "./ucal", str, str + sizeof(str)/2, NULL);
				}
				read(fifo, &average, sizeof(average));
				waitpid(pid, 0, 0);
				average /= (int)args[2];
				if (params.mp) {
					printf("\n*** '#define LATENCY_APIC %d' (IN USE %lu)\n\n", (int)params.latency_apic + average, params.latency_apic);
				} else {
					printf("\n*** '#define LATENCY_8254 %d' (IN USE %lu)\n\n", (int)params.latency_8254 + average, params.latency_8254);
				}
				break;
			}

			case 'a':
				if (!params.mp) {
					printf("*** APIC FREQUENCY CALIBRATION NOT AVAILABLE UNDER UP ***\n");
					break;
				}
			case 'b':
			case 'c': {
				unsigned long time, cpu_freq = 0, apic_freq = 0;
				struct times_t times;
				args[0] = FREQ_CAL;
				printf("Echo calibration time (seconds): ");
				getint(args[1]);
				printf("\n->>> HERE WE GO (PRINTING EVERY %lu SECONDS, press enter to end calibrating) <<<-\n\n", args[1]);
				rtai_srq(srq, (unsigned long)args);
				time = 0;
				while (1) {
					time += args[1];
					read(fifo, &times, sizeof(times));
					if (command == 'c' || command == 'b') {
						cpu_freq = (((double)times.cpu_time)*params.clock_tick_rate)/(((double)times.intrs)*params.latch) + 0.49999999999999999;
						printf("\n->>> MEASURED CPU_FREQ: %lu (Hz) [%lu (s)], IN USE %lu (Hz) <<<-\n", cpu_freq, time, params.cpu_freq);
					}
					if (params.mp && (command == 'a' || command == 'b')) {
						apic_freq = (((double)times.apic_time)*params.clock_tick_rate)/(((double)times.intrs)*params.latch) + 0.49999999999999999;
						printf("\n->>> MEASURED APIC_FREQ: %lu (Hz) [%lu (s)], IN USE %lu (Hz) <<<-\n", apic_freq, time, params.freq_apic);
					}
					if (poll(polls, 1, 0) > 0) {
						args[0] = END_FREQ_CAL;
						rtai_srq(srq, (unsigned long)args);
						if (command == 'c' || command == 'b') {
				 			printf("\n*** '#define CPU_FREQ %lu', IN USE %lu ***\n\n", cpu_freq, params.cpu_freq);
						}
						if (params.mp && (command == 'a' || command == 'b')) {
				 			printf("\n*** '#define APIC_FREQ %lu', IN USE %lu ***\n\n", apic_freq, params.freq_apic);
						}
						break;
					}
				}
				break;
			}

			case 'l': {
				time_t timestamp;
				struct tm *tm_timestamp;
				args[0] = BUS_CHECK;
				printf("Check period (microseconds): ");
				getint(args[1]);
				printf("Check threshold (microseconds): ");
				getint(args[2]);
				printf("Use parport (y/n)?: ");
				fgets(str, sizeof(str), stdin);
				str[strlen(str) - 1] = 0;
				if (!str[0] || (str[0] != 'y' && str[0] != 'n')) {
					printf("*** INVALID ***\n");
					break;
				}
				args[3] = str[0] == 'y' ? 1 : 0;
				printf("\n->>> HERE WE GO (press enter to end check) <<<-\n\n");
				args[1] *= 1000;
				args[2] *= 1000;
				rtai_srq(srq, (unsigned long)args);
				while (1) {
					int maxj;
					if (poll(polls, 2, 100) > 0) {
						if (polls[0].revents) {
							args[0] = END_BUS_CHECK;
							rtai_srq(srq, (unsigned long)args);
							break;
						}
						read(fifo, &maxj, sizeof(maxj));
						time(&timestamp);
						tm_timestamp = localtime(&timestamp);
						printf("%04d/%02d/%0d-%02d:%02d:%02d -> ", tm_timestamp->tm_year+1900, tm_timestamp->tm_mon+1, tm_timestamp->tm_mday, tm_timestamp->tm_hour, tm_timestamp->tm_min, tm_timestamp->tm_sec);
						printf("%d.%-3d (us)\n", maxj/1000, maxj%1000);
					}
				}
				break;
			}

			case 'e': {
				close(fifo);
				exit(0);
			}
		}
	}
	exit(0);
}
