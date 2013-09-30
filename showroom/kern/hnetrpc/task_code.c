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

#include <linux/module.h>
#include <linux/kernel.h>

#include <rtai_netrpc.h>

static char *ComNode = "127.0.0.1";
RTAI_MODULE_PARM(ComNode, charp);

static unsigned long comnode;

#define NUM_TASKS 4

static char *strs[NUM_TASKS] = { "Joey    ", "Johnny  ", "Dee Dee ", "Marky   "
};

static char *task[NUM_TASKS] = { "T1", "T2", "T3", "T4" };
static RT_TASK *tasks[NUM_TASKS];

static char *sem[NUM_TASKS] = { "S1", "S2", "S3", "S4" };
static SEM *sems[NUM_TASKS], *sync_sem, *prio_sem, *print_sem;

static MBX *mbx_in, *mbx_out;

#define TAKE_PRINT  RT_sem_wait(comnode, srvport, print_sem);
#define GIVE_PRINT  RT_sem_signal(comnode, srvport, print_sem);

extern atomic_t cleanup;

static void task_code(long task_no)
{
	int i, ret, srvport;
	char buf[9];

	buf[8] = 0;
	srvport = rt_request_hard_port(comnode);
rt_printk("TASK_NO %d GOT ITS EXEC COMNODE PORT %lx, %d\n", task_no, comnode, srvport);

	for (i = 0; i < 5; ++i)	{
		RT_sem_wait(comnode, srvport, sems[task_no]);
		TAKE_PRINT;
		rt_printk(strs[task_no]);
		GIVE_PRINT;
		if (task_no == NUM_TASKS - 1) {
			TAKE_PRINT;
			rt_printk("\n");
			GIVE_PRINT;
		}
		RT_sem_signal(comnode, srvport, sems[(task_no + 1)%NUM_TASKS]);
	}
	RT_sem_signal(comnode, srvport, sync_sem);
	RT_sem_wait(comnode, srvport, prio_sem);
	TAKE_PRINT;
	rt_printk(strs[task_no]);
	GIVE_PRINT;
	RT_sleep(comnode, srvport, 1000000000LL);
	RT_sem_wait_timed(comnode, srvport, prio_sem, (task_no + 1)*1000000000LL);
	TAKE_PRINT;
	rt_printk("sem timeout, task %d, %s\n", task_no, strs[task_no]);
	GIVE_PRINT;
	RT_sem_signal(comnode, srvport, sync_sem);

/* message queue stuff */
	if ((ret = RT_mbx_receive(comnode, srvport, mbx_in, buf, 8)) != 0) {
		TAKE_PRINT;
		rt_printk("RT_mbx_receive() failed with %d\n", ret);
		GIVE_PRINT;
	}
	TAKE_PRINT;
	rt_printk("\nreceived by task %d ", task_no);
	rt_printk(buf);
	GIVE_PRINT;
	RT_mbx_send(comnode, srvport, mbx_out, strs[task_no], 8);
/* test receive timeout */
	RT_sem_wait(comnode, srvport, sync_sem);
	if (RT_mbx_receive_timed(comnode, srvport, mbx_in, buf, 8, (task_no + 1)*1000000000LL)) {
		TAKE_PRINT;
		rt_printk("mbx timeout, task %d, %s\n", task_no, strs[task_no]);
		GIVE_PRINT;
	}
	TAKE_PRINT;
	rt_printk("\ntask %d complete\n", task_no);
	GIVE_PRINT;
rt_printk("TASK_NO %d REL ITS EXEC COMNODE PORT %lx, %d\n", task_no, comnode, srvport);
	rt_release_port(comnode, srvport);
	atomic_inc(&cleanup);
}

static RT_TASK init_thread;

static void init_code(long none)
{
	int i, srvport;

	while ((srvport = rt_request_hard_port(comnode)) <= 0);
rt_printk("TASK CODE GOT INIT COMNODE PORT %lx, %d\n", comnode, srvport);

	while (!(print_sem = RT_get_adr(comnode, srvport, "PRTSEM")));
	sync_sem = RT_get_adr(comnode, srvport, "SYNCSM");
	prio_sem = RT_get_adr(comnode, srvport, "PRIOSM");
	mbx_in   = RT_get_adr(comnode, srvport, "MBXIN");
	mbx_out  = RT_get_adr(comnode, srvport, "MBXOUT");
	for (i = 0; i < NUM_TASKS; i++) {
		sems[i] = RT_get_adr(comnode, srvport, sem[i]);
		tasks[i] = rt_named_task_init(task[i], task_code, i, 3000, NUM_TASKS - i, 0, 0);
		rt_task_resume(tasks[i]);
	}
rt_printk("TASK CODE REL INIT COMNODE PORT %lx, %d\n", comnode, srvport);
	rt_release_port(comnode, srvport);
	atomic_inc(&cleanup);
}

int init_module(void)
{
	comnode = ddn2nl(ComNode);
	rt_task_init(&init_thread, init_code, 0, 4000, 0, 0, 0);
	rt_task_resume(&init_thread);
	return 0;
}

void cleanup_module(void)
{
	int i;
	while (atomic_read(&cleanup) < (NUM_TASKS + 2)) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(HZ/10);
	}
	rt_task_delete(&init_thread);
	for (i = 0; i < NUM_TASKS; ++i) {
		rt_named_task_delete(tasks[i]);
	}
}
