/*
COPYRIGHT (C) 2008  Paolo Mantegazza <mantegazza@aero.polimi.it>

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


#include <rtai_lxrt.h>
#include <rtai_sem.h>
#include <rtai_msg.h>

int main (int argc, char **argv)
{
	rt_allow_nonroot_hrt();
	rt_task_init_schmod(nam2num("LOOPER"), 1, 0, 0, SCHED_FIFO, 0x2);
	rt_make_hard_real_time();
	if (rt_is_hard_timer_running() && argc == 2) {
		rt_task_make_periodic(rt_get_adr(nam2num("LOOPER")), rt_get_time(), nano2count(1000000));
	}
	if (argc == 1 || (argc == 2 && strstr(argv[1], "msg"))) {
		unsigned long msg;
		while (!rt_receive_if(NULL, &msg));
	} else {
		SEM *sem;
		sem = rt_sem_init(nam2num("KILSEM"), 0);
		while (rt_sem_wait_if(sem) <= 0);
		rt_sem_delete(sem);
	}
	rt_make_soft_real_time();
	rt_task_delete(NULL);
	return 0;
}
