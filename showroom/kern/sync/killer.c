/*
COPYRIGHT (C) 2008  Paolo Mantegazza (mantegazza@aero.polimi.it)

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


#include <sys/poll.h>

#include <rtai_lxrt.h>
#include <rtai_shm.h>
#include <rtai_msg.h>


int main (void)
{
	long *worst_lat;
        struct pollfd ufds = { 0, POLLIN, };

	rt_allow_nonroot_hrt();
	rt_task_init_schmod(nam2num("KILLER"), 1, 0, 0, SCHED_FIFO, 0xF);
        worst_lat = rt_shm_alloc(nam2num("WSTLAT"), sizeof(RTIME), USE_VMALLOC);
	if (rt_get_adr(nam2num("LOOPER"))) {
		while (1) {
	                if (poll(&ufds, 1, 1000)) {
				getchar();
				rt_send(rt_get_adr(nam2num("LOOPER")), 1UL);
				printf("KILLER ENDS NEVER ENDING TASK.\n");
				break;
			}
			printf("WORST LATENCY: %ld\n", *worst_lat);
			*worst_lat = 0;
		}
	}
        rt_shm_free(nam2num("WSTLAT"));
	rt_task_delete(NULL);
	return 0;
}
