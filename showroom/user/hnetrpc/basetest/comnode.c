/*
COPYRIGHT (C) 2001  Paolo Mantegazza <mantegazza@aero.polimi.it>

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
#include <sys/time.h>

#include <rtai_netrpc.h>

#define NUM_TASKS 4

static char *sem[NUM_TASKS] = { "S1", "S2", "S3", "S4" };
static SEM *sems[NUM_TASKS], *sync_sem, *prio_sem, *print_sem, *end_sem;

static MBX *mbx_in, *mbx_out;

static int init_module(void)
{
	int i;

	rt_set_oneshot_mode();
	start_rt_timer(0);
	sync_sem = rt_sem_init(nam2num("SYNCSM"), 0);
	prio_sem = rt_sem_init(nam2num("PRIOSM"), 0);
	if (!(mbx_in = rt_mbx_init(nam2num("MBXIN"), NUM_TASKS*8)) || !(mbx_out = rt_mbx_init(nam2num("MBXOUT"), NUM_TASKS*8))) {
		printf("could not create message queues\n");
		return 1;
	}
	for (i = 0; i < NUM_TASKS; ++i) {
		sems[i] = rt_sem_init(nam2num(sem[i]), 0);
	}
	end_sem = rt_typed_sem_init(nam2num("ENDSEM"), 0, CNT_SEM);
	print_sem = rt_typed_sem_init(nam2num("PRTSEM"), 1, BIN_SEM);
	return 0;
}

static void cleanup_module(void)
{
	int i;

	stop_rt_timer();
	for (i = 0; i < NUM_TASKS; ++i) {
		rt_sem_delete(sems[i]);
	}
	rt_sem_delete(print_sem);
	rt_sem_delete(prio_sem);
	rt_sem_delete(sync_sem);
	rt_sem_delete(end_sem);
	rt_mbx_delete(mbx_in);
	rt_mbx_delete(mbx_out);
}

void msleep(int ms)
{
	struct timeval timout;
	timout.tv_sec = 0;
	timout.tv_usec = ms*1000;
	select(1, NULL, NULL, NULL, &timout);
}

int main(void)
{
	int i;
	RT_TASK *task;

	if (!(task = rt_task_init(nam2num("CMNODE"), 0, 0, 0))) {
		printf("CANNOT INIT COMNODE TASK\n");
		exit(1);
	}

	init_module();
	for (i = 0; i < 7; i++) {
		rt_sem_wait(end_sem);
	}
	cleanup_module();
	rt_task_delete(task);

	exit(0);
}
