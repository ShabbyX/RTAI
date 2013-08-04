/*
COPYRIGHT (C) 2004  Paolo Mantegazza <mantegazza@aero.polimi.it>

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


#include <sys/mman.h>
#include <pthread.h>

#include <rtai_lxrt.h>
#include <rtai_sem.h>
#include <rtai_sysvmsg.h>

#define NTASKS  10

#define MAXSIZ  500

static volatile int end;

static void bfun(int t)
{
	RT_TASK *mytask;
	int smbx, rmbx[NTASKS], msg[MAXSIZ + 1], mtype, i, n;

	mytask = rt_thread_init(nam2num("BFUN"), 1, 0, SCHED_FIFO, 0xF);
	smbx = rt_msgget(nam2num("SMSG"), 0);
	for (i = 0; i < NTASKS; i++) {
		char mname[6] = "RMBX";
		mname[4] = i + '0';
		mname[5] = 0;
		rmbx[i] = rt_msgget(nam2num(mname), 0);
	}
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	while (!end) {
		rt_msgrcv_nu(smbx, &mtype, msg, sizeof(msg), 1, 0);
		n = 0;
		for (i = 1; i < MAXSIZ; i++) {
			n += msg[i];
		}
		if (msg[MAXSIZ] != n) {
			rt_printk("SERVER RECEIVED AN UNKNOWN MSG.\n");
			goto prem;
		}
		msg[1] = 0xFFFFFFFF;
		rt_msgsnd_nu(rmbx[msg[0]], 1, msg, 2*sizeof(int), 0);
	}
prem:
	rt_make_soft_real_time();
	rt_task_delete(mytask);
	printf("SERVER TASK ENDS.\n");
}

int main(void)
{
	RT_TASK *mytask;
	int smbx, bthread;
	char msg[] = "let's end the game";

	mytask = rt_thread_init(nam2num("MAIN"), 2, 0, SCHED_FIFO, 0xF);

	smbx    = rt_msgget(nam2num("SMSG"), 0x666 | IPC_CREAT);
	bthread = rt_thread_create(bfun, 0, 0x8000);

	printf("IF NOTHING HAPPENS IS OK, TYPE ENTER TO FINISH.\n");
	getchar();

	end = 1;
	rt_msgsnd_nu(smbx, 1, msg, sizeof(msg), 0);
	rt_thread_join(bthread);

	rt_task_delete(mytask);
	printf("MAIN TASK ENDS.\n");
	return 0;
}
