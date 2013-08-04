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

#define SLEEP_TIME  2000000

#define NTASKS  10

#define MAXSIZ  500

static SEM *barrier;

static volatile int cnt[NTASKS], end = -1;

static double randu(void)
{
	static int i = 783637;
	i = 125*i;
	i = i - (i/2796203)*2796203;
	return (-1.0 + (double)(i + i)/2796203.0);
}

static void mfun(int t)
{
	char tname[6] = "MFUN", mname[6] = "RMBX";
	RT_TASK *mytask;
	int smbx, rmbx, msg[MAXSIZ + 1], mtype, i;

	randu();
	tname[4] = mname[4] = t + '0';
	tname[5] = mname[5] = 0;
	mytask = rt_thread_init(nam2num(tname), t + 2, 0, SCHED_FIFO, 0xF);
	smbx = rt_msgget(nam2num("SMSG"), 0);
	rmbx = rt_msgget(nam2num(mname), 0);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	msg[0] = t;
	rt_sem_wait_barrier(barrier);
	while (end < t) {
		msg[MAXSIZ] = 0;
		for (i = 1; i < MAXSIZ; i++) {
			msg[MAXSIZ] += (msg[i] = MAXSIZ*randu());
		}
		if (rt_msgsnd_nu(smbx, 1, msg, sizeof(msg), 0)) {
			rt_printk("SEND FAILED, TASK: %d\n", t);
			goto prem;
		}
		msg[0] = msg[1] = 0;
		if (rt_msgrcv_nu(rmbx, &mtype, msg, sizeof(msg), 1, 0) < 0) {
			rt_printk("RECEIVE FAILED, TASK: %d\n", t);
			goto prem;
		}
		if (msg[0] != t || msg[1] != 0xFFFFFFFF) {
			rt_printk("WRONG REPLY TO TASK: %d.\n", t);
			goto prem;
		}
		cnt[t]++;
//		rt_printk("TASK: %d, OK (%d).\n", t, cnt[t]);
		rt_sleep(nano2count(SLEEP_TIME));
	}
prem:
	rt_make_soft_real_time();
	rt_task_delete(mytask);
	printf("TASK %d ENDS.\n", t);
}

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

	rt_sem_wait_barrier(barrier);
	while (end < NTASKS) {
		rt_msgrcv_nu(smbx, &mtype, msg, sizeof(msg), 1, 0);
		n = 0;
		for (i = 1; i < MAXSIZ; i++) {
			n += msg[i];
		}
		if (msg[MAXSIZ] != n) {
			rt_printk("SERVER RECEIVED AN UNKNOWN MSG.\n");
//			goto prem;
		}
if(end >= NTASKS) goto prem;
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
	int smbx, rmbx[NTASKS];
	int i, bthread, mthread[NTASKS];
	char msg[] = "let's end the game";

	mytask = rt_thread_init(nam2num("MAIN"), 2, 0, SCHED_FIFO, 0xF);
	barrier = rt_sem_init(rt_get_name(0), NTASKS + 1);

	smbx    = rt_msgget(nam2num("SMSG"), 0x666 | IPC_CREAT);
	for (i = 0; i < NTASKS; i++) {
		char mname[6] = "RMBX";
		mname[4] = i + '0';
		mname[5] = 0;
		rmbx[i] = rt_msgget(nam2num(mname), 0x666 | IPC_CREAT);
	}

	rt_set_oneshot_mode();
	start_rt_timer(0);

	bthread = rt_thread_create(bfun, 0, 0x8000);
	for (i = 0; i < NTASKS; i++) {
		mthread[i] = rt_thread_create(mfun, (void *)i, 0x8000);
	}

	printf("IF NOTHING HAPPENS IS OK, TYPE ENTER TO FINISH.\n");
	getchar();

	for (i = 0; i < NTASKS; i++) {
		end = i;
		rt_msgsnd_nu(rmbx[i], 1, msg, sizeof(msg), 0);
		rt_thread_join(mthread[i]);
	}

	end = NTASKS;
	rt_msgsnd_nu(smbx, 1, msg, sizeof(msg), 0);
	rt_thread_join(bthread);

	for (i = 0; i < NTASKS; i++) {
		rt_msgctl(rmbx[i], IPC_RMID, NULL);
		printf("TASK %d, LOOPS: %d.\n", i, cnt[i]);
	}
	rt_msgctl(smbx, IPC_RMID, NULL);

	stop_rt_timer();
	rt_sem_delete(barrier);
	rt_task_delete(mytask);
	printf("MAIN TASK ENDS.\n");
	return 0;
}
