/*
COPYRIGHT (C) 2005  Paolo Mantegazza <mantegazza@aero.polimi.it>

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
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/poll.h>

#include <rtai_lxrt.h>

#define USE_LINUX_SYSCALL

#define SYNC_ASYNC ASYNC_LINUX_SYSCALL

#define WAITIME  1     // seconds
#define BUFSIZE  1000  // bytes
#define HDRSIZE  50    // must be <= BUFSIZE
#define NRECS    100000

int main(void)
{
	char s[BUFSIZE];
	int i, fd;
	long long ll;
	float f;
	double d;
	RT_TASK *mytask;
	RTIME t;
	struct timeval timout;
	struct timespec tns;

 	if (!(mytask = rt_task_init_schmod(nam2num("HRTSK"), 0, 0, 0, SCHED_FIFO, 0x1))) {
		printf("CANNOT INIT TEST BUDDY TASK\n");
		exit(1);
	}
	rt_sync_async_linux_syscall_server_create(NULL, SYNC_LINUX_SYSCALL, NULL, NRECS + 1);
	rt_grow_and_lock_stack(40000);
	rt_make_hard_real_time();
	rt_task_use_fpu(mytask, 1);

#if 1
	printf("(SCANF) Input a: string, integer, long long, float, double: ");
	scanf("%s %i %lld %f %lf", s, &i, &ll, &f, &d);
	printf("(SELECT) Got string, integer, long long, now we wait %d s using select.\n", WAITIME);
	timout.tv_sec = WAITIME;
	timout.tv_usec = 0;
	select(1, NULL, NULL, NULL, &timout);
	printf("(PRINTF) Select expired and we print what we read: %s %i %lld %f %lf\n", s, i, ll, f, d);
	printf("(OPEN) Open a file.\n");
	fd = open("rtfile", O_RDWR | O_CREAT | O_TRUNC, 0666);
	printf("(WRITE) Write a %d bytes header of 'B's.\n", HDRSIZE);
	memset(s, 'B', HDRSIZE);
	write(fd, s, HDRSIZE);
	memset(s, 'A', BUFSIZE);
	printf("(WRITE) Write %d records of %d 'A's to the opened file.\n", NRECS, BUFSIZE);

#if SYNC_ASYNC == ASYNC_LINUX_SYSCALL
	printf("!!!!! The writing will be executed asynchronously !!!!!\n");
#else
	printf("!!!!! The writing will be executed synchronously !!!!!\n");
#endif
	rt_set_linux_syscall_mode(SYNC_ASYNC, NULL);
	t = rt_get_cpu_time_ns();
	for (i = 0; i < NRECS; i++) {
		write(fd, s, BUFSIZE);
	}
	rt_set_linux_syscall_mode(SYNC_LINUX_SYSCALL, NULL);

	printf("(PRINTF) WRITE TIME: %lld (ms).\n", (rt_get_cpu_time_ns() - t + 500000)/1000000);
	printf("(WRITE) Write a %d bytes trailer of 'E's.\n", HDRSIZE);
	memset(s, 'E', HDRSIZE);
	write(fd, s, HDRSIZE);
	printf("(SYNC) Sync file to disk.\n");
	sync();
	printf("(LSEEK) Position file at its beginning.\n");
	lseek(fd, 0, SEEK_SET);
	printf("(POLL) Got the beginning, now we wait %d s using poll.\n", WAITIME);
	poll(0, 0, WAITIME*1000);
	printf("(READ) Poll expired, read the first %d bytes (header + first 'A').\n", HDRSIZE + 1);
	read(fd, s, HDRSIZE + 1);
	s[HDRSIZE + 1] = 0;
	printf("(READ) Here is the header  %s.\n", s);
	lseek(fd, -1, SEEK_CUR);
	printf("(READ) Read the written %d MB of 'A's back.\n", BUFSIZE*NRECS);

#if SYNC_ASYNC == ASYNC_LINUX_SYSCALL
	printf("!!!!! The reading will be executed asynchronously !!!!!\n");
#else
	printf("!!!!! The reading will be executed asynchronously !!!!!\n");
#endif
	rt_set_linux_syscall_mode(ASYNC_LINUX_SYSCALL, NULL);
	t = rt_get_cpu_time_ns();
	for (i = 0; i < NRECS; i++) {
		read(fd, s, BUFSIZE);
	}
	rt_set_linux_syscall_mode(SYNC_LINUX_SYSCALL, NULL);

	printf("(PRINTF) READ TIME %lld (ms).\n", (rt_get_cpu_time_ns() - t + 500000)/1000000);
	lseek(fd, -1, SEEK_CUR);
	printf("(READ) Read the last %d bytes (last 'A' + trailer).\n", HDRSIZE + 1);
	read(fd, s, HDRSIZE + 1);
	s[HDRSIZE + 1] = 0;
	printf("(READ) Here is the trailer %s.\n", s);
	printf("(CLOSE) Close the file and end the test.\n");
	close(fd);
	printf("(NANOWAITIME) File closed, let's wait %d s using nanosleep.\n", WAITIME);
	tns.tv_sec = WAITIME;
	tns.tv_nsec = 0;
	nanosleep(&tns, NULL);
	printf("(PRINTF) Test done, exiting.\n");
#endif

	rt_make_soft_real_time();
	rt_task_delete(mytask);
	return 0;
}
