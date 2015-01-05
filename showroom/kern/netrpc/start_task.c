/*
COPYRIGHT (C) 2001  Paolo Mantegazza (mantegazza@aero.polimi.it),

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

static char *TaskNode = "127.0.0.1";
RTAI_MODULE_PARM(TaskNode, charp);

static unsigned long comnode, tasknode;

#define NUM_TASKS 4

static char *task[NUM_TASKS] = { "T1", "T2", "T3", "T4" };
static char *strs[NUM_TASKS] = { "Joey    ", "Johnny  ", "Dee Dee ", "Marky   " };
static char sync_str[] = "sync\n";

static char *sem[NUM_TASKS] = { "S1", "S2", "S3", "S4" };
static SEM *sems[NUM_TASKS], *sync_sem, *prio_sem, *print_sem;

static MBX *mbx_in, *mbx_out;

#define TAKE_PRINT  RT_sem_wait(comnode, srvport, print_sem);
#define GIVE_PRINT  RT_sem_signal(comnode, srvport, print_sem);

extern atomic_t cleanup;

static void start_task_code(long none)
{
	int i, srvport;
	char buf[9];

        while ((srvport = rt_request_port(tasknode)) <= 0);
rt_printk("START TASK GOT SYNC TASKNODE PORT %lx, %d\n", tasknode, srvport);
	while (!RT_get_adr(tasknode, srvport, task[NUM_TASKS - 1]));
rt_printk("START TASK REL SYNC TASKNODE PORT %lx, %d\n", tasknode, srvport);
        rt_release_port(tasknode, srvport);

        srvport = rt_request_port(comnode);
rt_printk("START TASK GOT ITS INIT AND EXEC COMNODE PORT %lx, %d\n", comnode, srvport);

	print_sem = RT_get_adr(comnode, srvport, "PRTSEM");
        sync_sem = RT_get_adr(comnode, srvport, "SYNCSM");
        prio_sem = RT_get_adr(comnode, srvport, "PRIOSM");
        mbx_in   = RT_get_adr(comnode, srvport, "MBXIN");
        mbx_out  = RT_get_adr(comnode, srvport, "MBXOUT");

	for (i = 0; i < NUM_TASKS; ++i) {
		sems[i] = RT_get_adr(comnode, srvport, sem[i]);
	}     

	buf[8] = 0;

	RT_sem_signal(comnode, srvport, sems[0]);
	for (i = 0; i < NUM_TASKS; ++i) {
		RT_sem_wait(comnode, srvport, sync_sem);
	}
	TAKE_PRINT; 
	rt_printk(sync_str);
	GIVE_PRINT;
	for (i = 0; i < NUM_TASKS; ++i) {
		RT_sem_signal(comnode, srvport, prio_sem);
	}
	TAKE_PRINT; 
	rt_printk("\n");
	GIVE_PRINT;
	for (i = 0; i < NUM_TASKS; ++i) {
		RT_sem_wait(comnode, srvport, sync_sem);
	}
	TAKE_PRINT; 
	rt_printk(sync_str);
	GIVE_PRINT;

	TAKE_PRINT; 
	rt_printk("testing message queues\n");
	GIVE_PRINT;
	for (i = 0; i < NUM_TASKS; ++i) {
		if (RT_mbx_send(comnode, srvport, mbx_in, strs[i], 8)) {
			TAKE_PRINT; 
			rt_printk("RT_mbx_send() failed\n");
			GIVE_PRINT;
		}
	}
	for (i = 0; i < NUM_TASKS; ++i) {
		RT_mbx_receive(comnode, srvport, mbx_out, buf, 8);
		TAKE_PRINT; 
		rt_printk("\nreceived from mbx_out: %s", buf); 
		GIVE_PRINT;
	}
	TAKE_PRINT; 
	rt_printk("\n");
	GIVE_PRINT;
	for (i = 0; i < NUM_TASKS; ++i) {
		RT_sem_signal(comnode, srvport, sync_sem);
	}
	TAKE_PRINT; 
	rt_printk("\ninit task complete\n"); 
	GIVE_PRINT;
        rt_release_port(comnode, srvport);
rt_printk("START TASK REL ITS EXEC COMNODE PORT %lx, %d\n", comnode, srvport);
	atomic_inc(&cleanup);
}

static RT_TASK thread;

int init_module(void)
{
        comnode  = ddn2nl(ComNode);
        tasknode = ddn2nl(TaskNode);
	rt_task_init(&thread, start_task_code, 0, 4000, 10, 0, 0);
	rt_task_resume(&thread);
	return 0;
}

void cleanup_module(void)
{
        while (atomic_read(&cleanup) < (NUM_TASKS + 2)) {
                current->state = TASK_INTERRUPTIBLE;
                schedule_timeout(HZ/10);
        }
	rt_task_delete(&thread);
}
