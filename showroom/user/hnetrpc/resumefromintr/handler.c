/*
COPYRIGHT (C) 2001  Paolo Mantegazza (mantegazza@aero.polimi.it)

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
#include <linux/module.h>
*/


#include <linux/module.h>

#include <asm/io.h>

#include <rtai_registry.h>
#include <rtai_netrpc.h>
#include "period.h"

static char *TaskNode = "127.0.0.1";
RTAI_MODULE_PARM(TaskNode, charp);

static int tasknode, taskport;
static RT_TASK sup_task;
static RT_TASK *rmt_task;
static SEM *rmt_sem;
static char run;

static volatile int overuns;

static void timer_tick(void)
{
	rt_times.tick_time = rt_times.intr_time;
	rt_times.intr_time = rt_times.tick_time + rt_times.periodic_tick;
	rt_set_timer_delay(0);
	if (rt_times.tick_time >= rt_times.linux_time) {
		if (rt_times.linux_tick) {
			rt_times.linux_time += rt_times.linux_tick;
		}
		rt_pend_linux_irq(TIMER_8254_IRQ);
	} 
	if (run) {
		if (rt_waiting_return(tasknode, taskport)) {
			overuns++;
		}
		rt_task_resume(&sup_task);
	}
}

static MBX mbx;

static void sup_fun(long none)
{
        while (tasknode && (taskport = rt_request_hard_port(tasknode)) <= 0);
	rt_mbx_receive(&mbx, &rmt_task, sizeof(rmt_task));
	rt_mbx_receive(&mbx, &rmt_sem, sizeof(rmt_sem));
	rt_mbx_receive(&mbx, &run, 1);

	do {
		rt_task_suspend(&sup_task);
		switch(run) {
			case 1: RT_sem_signal(tasknode, -taskport, rmt_sem);
				break;
			case 2: 
				RT_task_resume(tasknode, -taskport, rmt_task);
				break;
			case 3: 
				RT_send_if(tasknode, -taskport, rmt_task, run);
				break;
		}
	} while (rt_mbx_receive_if(&mbx, &run, 1));

       	rt_release_port(tasknode, taskport);
	rt_printk("ALL INTERRUPT MODULE FUNCTIONS TERMINATED\n");
}

int init_module(void)
{
        tasknode = ddn2nl(TaskNode);
	rt_mbx_init(&mbx, 1);
	rt_register(nam2num("HDLMBX"), &mbx, IS_MBX, 0);
        rt_task_init(&sup_task, sup_fun, 0, 2000, 1, 0, 0);
        rt_task_resume(&sup_task);
	rt_request_timer(timer_tick, imuldiv(PERIOD, FREQ_8254, 1000000000), 0);
	return 0;
}

void cleanup_module(void)
{
	rt_free_timer();
	rt_drg_on_adr(&mbx);
	rt_mbx_delete(&mbx);
	rt_task_delete(&sup_task);
	rt_printk("HANDLER INTERRUPT MODULE REMOVED, OVERUNS %d\n", overuns);
}
