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

#define NUM_TASKS 4

static char *sem[NUM_TASKS] = { "S1", "S2", "S3", "S4" };
static SEM *sems[NUM_TASKS], *sync_sem, *prio_sem, *print_sem;

static MBX *mbx_in, *mbx_out;

atomic_t cleanup;

int init_module(void)
{
	int i;

	rt_set_oneshot_mode();
	start_rt_timer(0);
	sync_sem = rt_named_sem_init("SYNCSM", 0);
	prio_sem = rt_named_sem_init("PRIOSM", 0);
	if (!(mbx_in = rt_named_mbx_init("MBXIN", NUM_TASKS*8)) || !(mbx_out = rt_named_mbx_init("MBXOUT", NUM_TASKS*8))) {
		rt_printk("could not create message queues\n");
		return 1;
	}
	for (i = 0; i < NUM_TASKS; ++i) {
		sems[i] = rt_named_sem_init(sem[i], 0);
	}     
	print_sem = rt_typed_named_sem_init("PRTSEM", 1, BIN_SEM);
	return 0;
}

void cleanup_module(void)
{
	int i;

	while (atomic_read(&cleanup) < (NUM_TASKS + 2)) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(HZ/10);
	}
	stop_rt_timer();
	for (i = 0; i < NUM_TASKS; ++i) {
		rt_named_sem_delete(sems[i]);
	}
	rt_named_sem_delete(print_sem);
	rt_named_sem_delete(prio_sem);
	rt_named_sem_delete(sync_sem);
	rt_named_mbx_delete(mbx_in);
	rt_named_mbx_delete(mbx_out);
}

EXPORT_SYMBOL(cleanup);
