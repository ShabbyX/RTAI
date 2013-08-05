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

int main (void)
{
	rt_allow_nonroot_hrt();
	rt_task_init_schmod(nam2num("KILLER"), 1, 0, 0, SCHED_FIFO, 0xF);
	if (rt_get_adr(nam2num("LOOPER"))) {
		rt_make_hard_real_time();
		if (rt_get_adr(nam2num("KILSEM"))) {
			rt_task_resume(rt_get_adr(nam2num("LOOPER")));
			rt_sem_signal(rt_get_adr(nam2num("KILSEM")));
			rt_make_soft_real_time();
			printf("KILLER ENDS NEVER ENDING TASK USING A SEM.\n");
		} else {
			rt_task_resume(rt_get_adr(nam2num("LOOPER")));
			rt_send(rt_get_adr(nam2num("LOOPER")), 1UL);
			rt_make_soft_real_time();
			printf("KILLER ENDS NEVER ENDING TASK USING A MSG.\n");
		}
	}
	rt_task_delete(NULL);
	return 0;
}
