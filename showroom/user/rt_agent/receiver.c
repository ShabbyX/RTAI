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


#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sched.h>
#include <rtai_shm.h>
#include <rtai_msg.h>

int main(void)
{
	RT_TASK *receiving_task;
	RT_TASK *agentask;
	int i, *shm;
	unsigned long msg, chksum;

	receiving_task = rt_task_init_schmod(nam2num("RTSK"), 0, 0, 0, SCHED_FIFO, 0xF);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();
	agentask = rt_get_adr(nam2num("ATSK"));
	shm = rt_shm_alloc(nam2num("MEM"), 0, 0);
	while(1) {
		printf("RECEIVING TASK RPCS TO AGENT TASK %x\n", 0xaaaaaaaa);
		rt_rpc(agentask, 0xaaaaaaaa, &msg);
		printf("AGENT TASK RETURNED %lx\n", msg);
		if (msg != 0xeeeeeeee) {
			chksum = 0;
			for (i = 1; i <= shm[0]; i++) {
				chksum += shm[i];
			}
			printf("RECEIVING TASK: CHECKSUM = %lx\n", chksum);
			if (chksum != shm[shm[0] + 1]) {
				printf("RECEIVING TASK: WRONG SHMEM CHECKSUM\n");
			}
			printf("RECEIVING TASK SENDS TO AGENT TASK %x\n", 0xaaaaaaaa);
			rt_send(agentask, 0xaaaaaaaa);
		} else {
			printf("RECEIVING TASK DELETES ITSELF\n");
			rt_task_delete(receiving_task);
			printf("END RECEIVING TASK\n");
			exit(1);

		}
	}
	return 0;
}
