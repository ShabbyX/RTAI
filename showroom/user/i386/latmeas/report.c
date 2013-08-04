/*
 * Original copyright:
 *
 * COPYRIGHT (C) 2004 Akamina Technologies Inc <www.akamina.com>
 *
 * This module includes the code to collect timer data sent to a mailbox
 * by a data acquisition process. The daq process sends pairs of latencies
 * specified in timer counts. This process converts the latencies to
 * microseconds and counts the number of times that latencies fall into
 * the range of each of a number of bins. The bin counts are written into
 * files once all of the samples have been collected.
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
 * This is an extensive rewrite for carrying out the same test on a generic
 * PC, by Paolo Mantegazza <mantegazza@aero.polimi.it>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <sys/mman.h>

#include <rtai_mbx.h>

#define NUM_PERIODS      100

#define isr_lat_range    1000
#define sched_lat_range  5000

static int end;
static void endme(int dummy) { end = 1; }

int main(int argc, char *argv[])
{
	RT_TASK *reptsk;
	MBX *mbx;
	int num_periods = NUM_PERIODS;
	int isr_cnt[isr_lat_range + 1], sched_cnt[sched_lat_range + 1];
	int i, k;
	struct { int isr; int sched; } latency;
	FILE *latency_file;

	signal(SIGINT, endme);

	if (!(reptsk = rt_task_init_schmod(nam2num("REPTSK"), 50, 0, 0, SCHED_FIFO, 0xF))) {
		printf("Error creating report task\n");
		return 1;
	}

 	if (!(mbx = rt_get_adr(nam2num("LATMBX")))) {
		printf("Cannot find mailbox\n");
		rt_task_delete(reptsk);
		return 1;
	}

	if (argc > 1) {
		num_periods = atoi(argv[1]);
	}
	memset(isr_cnt,   0, (isr_lat_range + 1)*sizeof(int));
	memset(sched_cnt, 0, (sched_lat_range + 1)*sizeof(int));

	printf("\nGathering latency data for %d periods.\n\n", num_periods);
	mlockall(MCL_CURRENT | MCL_FUTURE);
//	rt_make_hard_real_time();
	for(i = 0; i < num_periods && !end; i++) {
		rt_mbx_receive(mbx, &latency, sizeof(latency));
		isr_cnt[latency.isr < isr_lat_range ? latency.isr : isr_lat_range]++;
		sched_cnt[latency.sched < sched_lat_range ? latency.sched : sched_lat_range]++;
	}
	num_periods = i;

	rt_make_soft_real_time();
	rt_task_delete(reptsk);

	if (!(latency_file = fopen( "latencies.txt", "w+"))) {
		printf( "Error opening file: latencies.txt.\n");
		return 1;
	}
	fprintf(latency_file, "Number of periods: %d\n", num_periods);
	fprintf(latency_file, "Resolution: 1 us\n\n");

	fprintf(latency_file, "Interrupt latency report:\n");
	fprintf(latency_file, "Latency range: %d (us)\n", isr_lat_range);
	fprintf(latency_file, "Out of range: %d\n", isr_cnt[isr_lat_range]);
	for(i = k = 0; i <= isr_lat_range; i++) {
		fprintf(latency_file, "%d: %d\n", i, isr_cnt[i]);
		if ((k += isr_cnt[i]) >= num_periods) {
			printf("MAX ISR LATENCY: %d (us).\n", i);
			break;
		}
	}

	fprintf(latency_file, "\nSchedule latency report:\n");
	fprintf(latency_file, "Latency range: %d (us)\n", sched_lat_range);
	fprintf(latency_file, "Out of range: %d\n", sched_cnt[sched_lat_range]);
	for(i = k = 0; i <= sched_lat_range; i++) {
		fprintf(latency_file, "%d: %d\n", i, sched_cnt[i]);
		if ((k += sched_cnt[i]) >= num_periods) {
			printf("MAX SCHED LATENCY: %d (us).\n\n", i);
			break;
		}
	}
	fclose(latency_file);

	return 0;
}
