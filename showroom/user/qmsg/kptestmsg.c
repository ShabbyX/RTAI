/*
COPYRIGHT (C) 2005-2013  Paolo Mantegazza (mantegazza@aero.polimi.it)

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


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");

#include <rtai_sched.h>
#include <rtai_sem.h>
#include <rtai_malloc.h>
#include <rtai_tbx.h>

#define SLEEP_TIME  1000000

#define NTASKS  10

#define MAXSIZ  500

static SEM barrier;

static volatile int cnt[NTASKS], end = -1;

static double randu(void)
{
	static int i = 783637;
	i = 125*i;
	i = i - (i/2796203)*2796203;
	return (-1.0 + (double)(i + i)/2796203.0);
}

static void mfun(long t)
{
	char tname[6] = "MFUN", mname[6] = "RMBX";
	RT_MSGQ *smbx, *rmbx;
	int *msg, mtype, i;

	msg  = rt_malloc((MAXSIZ + 1)*sizeof(int));

	randu();
	tname[4] = mname[4] = t + '0';
	tname[5] = mname[5] = 0;
	smbx = rt_named_msgq_init("SMSG", 0, 0);
	rmbx = rt_named_msgq_init(mname, 0, 0);

	msg[0] = t;
	rt_sem_wait_barrier(&barrier);
	while (end < t) {
		msg[MAXSIZ] = 0;
		for (i = 1; i < MAXSIZ; i++) {
			msg[MAXSIZ] += (msg[i] = MAXSIZ*randu()); 
		}
		if (rt_msg_send(smbx, msg, sizeof(int)*(MAXSIZ + 1), 1)) {
			rt_printk("SEND FAILED, TASK: %d\n", t);
			goto prem;
		}
		msg[0] = msg[1] = 0;
		if (rt_msg_receive(rmbx, msg, sizeof(int)*(MAXSIZ + 1), &mtype) < 0) {
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
	rt_free(msg);
	rt_printk("TASK %d ENDS.\n", t);
}

static RT_MSGQ *smbx, *rmbx[NTASKS];
static RT_TASK mthread[NTASKS];

int init_module(void)
{
	int i;

	rt_sem_init(&barrier, NTASKS);

	smbx = rt_named_msgq_init("SMSG", NTASKS, 0);
	for (i = 0; i < NTASKS; i++) {
		char mname[6] = "RMBX";
		mname[4] = i + '0';
		mname[5] = 0;
		rmbx[i] = rt_named_msgq_init(mname, 1, 0);
	}

	rt_set_oneshot_mode();
	start_rt_timer(0);

	for (i = 0; i < NTASKS; i++) {
		rt_kthread_init(&mthread[i], mfun, i, 0x8000, i + 2, 1, 0);
		rt_task_resume(&mthread[i]);
	}

	return 0;
}

void cleanup_module(void)
{
	char msg[] = "let's end the game";
	int i;

	for (i = 0; i < NTASKS; i++) {
		end = i;
		rt_msg_send(rmbx[i], msg, sizeof(msg), 1);
	}

	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout(HZ);

	for (i = 0; i < NTASKS; i++) {
		rt_named_msgq_delete(rmbx[i]);
		printk("TASK %d, LOOPS: %d.\n", i, cnt[i]); 
	}
	rt_named_msgq_delete(smbx);

	stop_rt_timer();
	rt_sem_delete(&barrier);

	for (i = 0; i < NTASKS; i++) {
		rt_task_delete(&mthread[i]);
	}

	printk("MODULE ENDS.\n");
}
