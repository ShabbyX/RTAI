/*
COPYRIGHT (C) 2004  Paolo Mantegazza (mantegazza@aero.polimi.it)

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
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <sys/poll.h>

//#include <rtai_lxrt.h>

int main(int argc, char *argv[])
{
	int pid, ret, errsv;

	if (argc != 2) {
		printf("\nNO TASK TO MAKE SOFT HAS BEEN GIVEN, USAGE:\nmakesoft <pid>.\n\n");
		return 1;
	}
	pid = strtol(argv[1], (char **)NULL, 10);
	ret = kill(pid, SIGUSR1);
	errsv = errno;
	printf("KILL RET %d %d %d\n", pid, ret, errsv);
	return 0;
#if 0
	RT_TASK *mytask, *task;
	int pid;

	if (argc != 2) {
		printf("\nNO TASK TO TERMINATE OR MAKE SOFT HAS BEEN GIVEN, USAGE:\nmakesoft <pid>; (pid > 0 task made soft, pid < 0 task terminated).\n\n");
		return 1;
	}
 	if (!(mytask = rt_task_init(nam2num("KILTSK"), 1000000, 0, 0))) {
		printf("CANNOT INIT LXRT EXTENSION FOR MAKESOFT.\n");
		return 1;
	}
	pid = strtol(argv[1], (char **)NULL, 10);
#if 0 // the previous way
	printf("%s HARD LXRT REAL TIME TASK, PID = %d.\n", pid < 0 ? "TERMINATING" : "MAKING IT SOFT THE", pid);
	rt_force_task_soft(abs(pid));
	return 0;
#else
	if (pid && (task = rt_force_task_soft(abs(pid)))) {
		while (rt_is_hard_real_time(task)) {
			poll(0, 0, 30);
		}
		if (pid < 0) {
			kill(abs(pid), SIGKILL);
		}
		printf("HARD LXRT REAL TIME TASK, PID = %d, %s.\n", pid, pid < 0 ? "TERMINATED" : "MADE SOFT");
	} else {	
		printf("TASK PID: %d, NOT HARD REAL TIME OR FAKY.\n", pid);
	}
#endif
	rt_task_delete(mytask);
#endif
	return 0;
}
