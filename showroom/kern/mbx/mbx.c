/*
COPYRIGHT (C) 1999  Paolo Mantegazza (mantegazza@aero.polimi.it)

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
#include <linux/module.h>

#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_mbx.h>

MODULE_LICENSE("GPL");

#define MBX_TYPE  RES_Q
//#define MBX_TYPE  FIFO_Q
//#define MBX_TYPE  PRIO_Q

#define TIMEOUT     1000000

#define SLEEP_TIME  100000

#define STACK_SIZE  2000

static volatile int PREMATURE = 1;
static volatile int cpu_used[NR_RT_CPUS];
static volatile int premature;

static RT_TASK mtask[2], btask;

static MBX smbx;
static MBX rmbx[2];

static unsigned long long name[2] = { 0x1122334455667788LL, 0x99aabbccddeeff00LL };

static int mstat[2];
static int bstat;
static int meant[2];

static void mfun(long t)
{
	RTIME time;
	unsigned long long msg;
	int size, maxt = 0, itime;

	while (1) {
		time = rt_get_cpu_time_ns();
		cpu_used[hard_cpu_id()]++;

		mstat[t] = 's';
		if ((size = rt_mbx_send_timed(&smbx, &name[t], sizeof(long long), nano2count(TIMEOUT)))) {
			rt_printk("SEND TIMEDOUT TASK: %d, UNSENT: %d, STAT: %c %c %c, OVERTIME: %d.\n", t, size, mstat[0], mstat[1], bstat, (int)(rt_get_cpu_time_ns() - time));
			goto prem;
		}

		msg = 0;
		mstat[t] = 'r';
		if ((size = rt_mbx_receive_timed(&rmbx[t], &msg, sizeof(msg), nano2count(TIMEOUT)))) {
			rt_printk("RECEIVE TIMEDOUT TIME: %d, NOTRECEIVED: %d, MSG: %x%x, STAT: %c %c %c, OVERTIME: %d.\n", t, size, ((int *)&msg)[1], ((int *)&msg)[0], mstat[0], mstat[1], bstat, (int)(rt_get_cpu_time_ns() - time));
			goto prem;
		}

		if (msg != 0xccccccccccccccccLL) {
                        rt_printk("WRONG REPLY TO TASK: %d, MSG: %x %x.\n", t, ((int *)&msg)[0], ((int *)&msg)[1]);
                        goto prem;
		}
		mstat[t] = 'd';
		if ((itime = rt_get_cpu_time_ns() - time) > maxt) {
			rt_printk("TASK: %d, MAXWAITIME: %d.\n", t, maxt = itime);
		}
		meant[t] = (itime + meant[t]) >> 1;
		rt_sleep(nano2count(SLEEP_TIME));
	}

prem:
	premature = PREMATURE;
	rt_printk("TASK # %d ENDS PREMATURELY\n", t);
}

static void bfun(long t)
{
	unsigned long long msg;
	unsigned long long name = 0xccccccccccccccccLL;

	while (1) {
		cpu_used[hard_cpu_id()]++;

		msg = 0LL;
		bstat = 'r';
		rt_mbx_receive(&smbx, &msg, sizeof(msg));
		if (msg == 0x1122334455667788LL) {
			t = 0;
		} else {
			if (msg == 0x99aabbccddeeff00LL) {
				t = 1;
			} else {
				rt_printk("SERVER RECEIVED AN UNKNOWN MSG: %x%x, STAT: %c %c %c.\n", ((int *)&msg)[1], ((int *)&msg)[0], mstat[0], mstat[1], bstat);
				t = 0;
				goto prem;
			}
		}
		bstat = '0' + t;
		rt_mbx_send(&rmbx[t], &name, sizeof(name));
	}
prem:
	premature = PREMATURE;
	rt_printk("SERVER TASK ENDS PREMATURELY.\n");
}

int init_module(void)
{
	rt_typed_mbx_init(&smbx, 5, MBX_TYPE); //5
	rt_typed_mbx_init(&rmbx[0], 1, MBX_TYPE); //1
	rt_typed_mbx_init(&rmbx[1], 3, MBX_TYPE); //3
	rt_task_init(&btask, bfun, 0, STACK_SIZE, 0, 0, 0);
	rt_task_init(&mtask[0], mfun, 0, STACK_SIZE, 1, 0, 0);
	rt_task_init(&mtask[1], mfun, 1, STACK_SIZE, 1, 0, 0);
	rt_set_oneshot_mode();
	start_rt_timer(nano2count(SLEEP_TIME));
	rt_task_resume(&btask);
	rt_task_resume(&mtask[0]);
	rt_task_resume(&mtask[1]);
	return 0;
}

void cleanup_module(void)
{
	int cpuid;

	PREMATURE = 0;
	rt_mbx_delete(&smbx);
	rt_mbx_delete(&rmbx[0]);
	rt_mbx_delete(&rmbx[1]);
	stop_rt_timer();
	rt_task_delete(&mtask[0]);
	rt_task_delete(&mtask[1]);
	rt_task_delete(&btask);
	printk("\n\nCPU USE SUMMARY (AVRG WAIT TIME):\n");
	for (cpuid = 0; cpuid < NR_RT_CPUS; cpuid++) {
		printk("# %d -> %d (%d)\n", cpuid, cpu_used[cpuid], meant[cpuid]);
	}
	printk("END OF CPU USE SUMMARY\n\n");
	if (premature) {
		printk("\n\nPREMATURE END\n");
	}
}
