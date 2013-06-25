/*
 * Copyright (C) 2003 Paolo Mantegazza <mantegazza@aero.polimi.it>
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

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/io.h>
#include <getopt.h>
#include <pthread.h>
#include <math.h>

#include <stdio.h>
#include <stdlib.h>
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
#include <rtai_fifos.h>
#include "calibrate.h"

struct option options[] = {
    { "help",       0, 0, 'h' },
    { "r8254",      0, 0, 'r' },
    { "kernel",     0, 0, 'k' },
    { "user",       0, 0, 'u' },
    { "period",     1, 0, 'p' },
    { "time",       1, 0, 't' },
    { "cpu",        0, 0, 'c' },
    { "apic",       0, 0, 'a' },
    { "both",       0, 0, 'b' },
    { "scope",      1, 0, 's' },
    { "interrupt",  0, 0, 'i' },
    { NULL,         0, 0,  0 }
};

void print_usage(void)
{
    fputs(
	("\n*** i386 calibrations ***\n"
	 "\n"
	 "OPTIONS:\n"
	 "  -h, --help\n"
	 "      print usage\n"
	 "  -r, --r8254\n"
	 "      calibrate 8254 oneshot programming type\n"
	 "  -k, --kernel\n"
	 "      oneshot latency calibrated for scheduling kernel space tasks\n"
	 "  -u, --user\n"
	 "      oneshot latency calibrated for scheduling user space tasks\n"
	 "  -p <period (us)>, --period <period (us)>\n"
	 "      the period of the underlying hard real time task/intr, default 100 (us)\n"
	 "  -t <duration (s)>, --time <duration (s)>\n"
	 "      set the duration of the requested calibration, default 5 (s)\n"
	 "  -c, --cpufreq\n"
	 "      calibrate cpu frequency\n"
	 "  -a, --apic\n"
	 "      calibrate apic frequency\n"
	 "  -b, --both\n"
	 "      calibrate both apic and cpu frequency\n"
	 "  -i, --interrupt\n"
	 "      check worst case interrupt locking/contention on your PC\n"
	 "  -s<y/n>, --scope<y/n>\n"
	 "      toggle parport bit to monitor scheduling on a scope, default y(es)\n"
	 "\n")
	, stderr);
}

static void endme(int dummy) { }

static void unload_kernel_helper (void)

{
    int rv;
    char modunload[1024];

    snprintf(modunload,sizeof(modunload),"/sbin/rmmod %s >/dev/null 2>&1",KERNEL_HELPER_PATH);
    rv = system(modunload);
}

int main(int argc, char *argv[])
{
	int srq, fifo, command, commands[20], ncommands, c, option_index, i, rv;
	struct params_t params;
	char str[80], nm[RTF_NAMELEN+1], modload[1024];
	unsigned long period = 100, duration = 5, parport = 'y';
	unsigned long args[MAXARGS];
	struct pollfd polls[2] = { { 0, POLLIN }, { 0, POLLIN } };

	option_index = ncommands = 0;
	while (1) {
		if ((c = getopt_long(argc, argv, "hrkucabip:t:s:", options, &option_index)) == -1) {
			break;
		}
		switch(c) {
			case 'h': {
				print_usage();
				exit(0);
			}
			case 'r':
			case 'u':
			case 'c':
			case 'b':
			case 'a':
			case 'i':
			case 'k': {
				commands[ncommands++] = c;
				break;
			}
			case 'p': {
				period = atoi(optarg);
				break;
			}
			case 't': {
				duration = atoi(optarg);
				break;
			}
			case 's': {
				parport = optarg[0] == 'y' ? 'y' : 'n';
				break;
			}
		}
	}
	if (ncommands <= 0) {
		printf("\n*** NOTHING TO BE DONE ***\n");
		exit(0);
	}

	atexit(&unload_kernel_helper);

	snprintf(modload,sizeof(modload),"/sbin/insmod %s >/dev/null 2>&1",KERNEL_HELPER_PATH);
	rv = system(modload);

	signal(SIGINT, endme);

	srq = rtai_open_srq(CALSRQ);
	if ((fifo = open(rtf_getfifobyminor(0,nm,sizeof(nm)), O_RDONLY)) < 0) {
		printf("Error opening %s\n",nm);
		exit(1);
	}
	polls[1].fd = fifo;

	args[0] = GET_PARAMS;
	rtai_srq(srq, (unsigned long)args);
	rv = read(fifo, &params, sizeof(params));
	printf("\n*** NUMBER OF REQUESTS: %d. ***\n", ncommands);

	for (i = 0; i < ncommands; i++) {
		switch (command = commands[i]) {

			case 'r': {
				args[0] = CAL_8254;
			 	printf("\n*** '#define SETUP_TIME_8254 %lu', IN USE %lu ***\n\n", (unsigned long) rtai_srq(srq, (unsigned long)args), params.setup_time_8254);
				break;
			}

			case 'k': {
				int average;
				args[0] = KLATENCY;
				args[1] = period;
				args[2] = duration;
				printf("\n*** KERNEL SPACE LATENCY CALIBRATION, wait %lu seconds for it ... ***\n", args[2]);
				args[2] = (1000000*args[2])/args[1];
				args[1] *= 1000;
				rtai_srq(srq, (unsigned long)args);
				rv = read(fifo, &average, sizeof(average));
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
				args[1] = period;
				args[2] = duration;
				printf("\n*** USER SPACE LATENCY CALIBRATION, wait %lu seconds for it ... ***\n", args[2]);
				args[2] = (1000000*args[2])/args[1];
				args[1] *= 1000;
				sprintf(str, "%lu", args[1]);
				sprintf(str + sizeof(str)/2, "%lu", args[2]);
				pid = fork();
				if (!pid) {
					execl(USER_HELPER_PATH, USER_HELPER_PATH, str, str + sizeof(str)/2, NULL);
					_exit(1);
				}
				rv = read(fifo, &average, sizeof(average));
				waitpid(pid, 0, 0);
				if (kill(pid,0) < 0)
				    {
//				    printf("\n*** Cannot execute calibration helper %s -- aborting\n",USER_HELPER_PATH);
//				    exit(1);
				    }
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
				args[1] = duration;
				printf("\n->>> FREQ CALIB (PRINTING EVERY %lu SECONDS, press enter to end calibrating) <<<-\n\n", args[1]);
				rtai_srq(srq, (unsigned long)args);
				time = 0;
				while (1) {
					time += args[1];
					if (poll(polls, 2, 1000*duration) > 0 &&polls[0].revents) {
						getchar();
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
					rv = read(fifo, &times, sizeof(times));
					if (command == 'c' || command == 'b') {
						cpu_freq = (((double)times.cpu_time)*params.clock_tick_rate)/(((double)times.intrs)*params.latch) + 0.49999999999999999;
						printf("\n->>> MEASURED CPU_FREQ: %lu (Hz) [%lu (s)], IN USE %lu (Hz) <<<-\n", cpu_freq, time, params.cpu_freq);
					}
					if (params.mp && (command == 'a' || command == 'b')) {
						apic_freq = (((double)times.apic_time)*params.clock_tick_rate)/(((double)times.intrs)*params.latch) + 0.49999999999999999;
						printf("\n->>> MEASURED APIC_FREQ: %lu (Hz) [%lu (s)], IN USE %lu (Hz) <<<-\n", apic_freq, time, params.freq_apic);
					}
				}
				break;
			}

			case 'i': {
				time_t timestamp;
				struct tm *tm_timestamp;
				args[0] = BUS_CHECK;
				args[1] = period;
				args[2] = 1;
				args[3] = parport;
				printf("\n->>> INTERRUPT LATENCY CHECK (press enter to end check) <<<-\n\n");
				args[1] *= 1000;
				args[2] *= 1000;
				rtai_srq(srq, (unsigned long)args);
				while (1) {
					int maxj;
					if (poll(polls, 2, 100) > 0) {
						if (polls[0].revents) {
							getchar();
							args[0] = END_BUS_CHECK;
							rtai_srq(srq, (unsigned long)args);
							break;
						}
						rv = read(fifo, &maxj, sizeof(maxj));
						time(&timestamp);
						tm_timestamp = localtime(&timestamp);
						printf("%04d/%02d/%0d-%02d:%02d:%02d -> ", tm_timestamp->tm_year+1900, tm_timestamp->tm_mon+1, tm_timestamp->tm_mday, tm_timestamp->tm_hour, tm_timestamp->tm_min, tm_timestamp->tm_sec);
						printf("%d.%-3d (us)\n", maxj/1000, maxj%1000);
					}
				}
				break;
			}

		}
	}
	exit(0);
}
