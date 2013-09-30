/*
 * COPYRIGHT (C) 2008  Bernhard Pfund (bernhard@chapter7.ch)
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

#include <rtai_mbx.h>

static volatile int end;
static void endme(int dummy) { end = 1;}

int main(void)
{
	MBX *mbx;
	time_t timestamp;
	struct tm *tm_timestamp;
	struct sample { unsigned long cnt; RTIME tx; RTIME rx;} samp;

	signal(SIGINT, endme);

	rt_task_init_schmod(nam2num("RTNCHK"), 0, 0, 0, SCHED_OTHER, 0xF);
	mbx = rt_typed_named_mbx_init("MYMBX", 0, FIFO_Q);

	while (!end) {
		rt_mbx_receive(mbx, &samp, sizeof(samp));
		if (!samp.tx) {
			printf("%lu - MAXJ = %lld\n", samp.cnt, samp.rx);
		} else {
			time(&timestamp);
			tm_timestamp=localtime(&timestamp);
			printf("%lu - TM: %04d/%02d/%0d %02d:%02d:%02d.\n", samp.cnt, tm_timestamp->tm_year+1900, tm_timestamp->tm_mon+1, tm_timestamp->tm_mday, tm_timestamp->tm_hour, tm_timestamp->tm_min, tm_timestamp->tm_sec);
			timestamp = samp.tx/1000000000;  // let's see ours
			tm_timestamp=localtime(&timestamp);
			printf("%lu - TX: %04d/%02d/%0d %02d:%02d:%02d, %lld.\n", samp.cnt, tm_timestamp->tm_year+1900, tm_timestamp->tm_mon+1, tm_timestamp->tm_mday, tm_timestamp->tm_hour, tm_timestamp->tm_min, tm_timestamp->tm_sec, samp.tx/1000);
			timestamp = samp.rx/1000000000;  // let's see ours
			tm_timestamp=localtime(&timestamp);
			printf("%lu - RX: %04d/%02d/%0d %02d:%02d:%02d, %lld.\n", samp.cnt, tm_timestamp->tm_year+1900, tm_timestamp->tm_mon+1, tm_timestamp->tm_mday, tm_timestamp->tm_hour, tm_timestamp->tm_min, tm_timestamp->tm_sec, samp.rx/1000);
		}
	}

	rt_named_mbx_delete(mbx);

	return 0;
}
