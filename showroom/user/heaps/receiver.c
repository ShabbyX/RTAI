/*
COPYRIGHT (C) 2004  Paolo Mantegazza (mantegazza@aero.polimi.it)

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

#define SUPRT USE_VMALLOC

//#define DISPLAY
#ifdef  DISPLAY
#define PRINTF(fmt, args...)  rt_printk(fmt, ##args)
#else
#define PRINTF(fmt, args...)  printf(fmt, ##args)
#endif

int main(void)
{
	RT_TASK *receiving_task;
	RT_TASK *agentask;
	int i, *shm;
	void *heap;
	unsigned int msg, chksum, count;

	receiving_task = rt_task_init_schmod(nam2num("RTSK"), 0, 0, 0, SCHED_FIFO, 0xF);
	heap = rt_heap_open(nam2num("HEAP"), 0, SUPRT);
	rt_global_heap_open();
	shm = rt_named_halloc(nam2num("MEM"), 0);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();
	agentask = rt_get_adr(nam2num("ATSK"));
	rt_malloc(87);
	count = 0;
	while(1) {
		RTIME t;
		void *inloop1, *inloop2;
		t = rt_get_cpu_time_ns();
		inloop1 = rt_named_halloc(nam2num("pallin"), ++count);
		inloop2 = rt_malloc(count + 5);
		PRINTF("RECEIVER ALLOC TIME %d %d\n", (int)(rt_get_cpu_time_ns() - t), count + count + 5);
		PRINTF("RECEIVING TASK RPCS TO AGENT TASK %x\n", 0xaaaaaaaa);
		rt_rpc(agentask, 0xaaaaaaaa, &msg);
		PRINTF("AGENT TASK RETURNED %x\n", msg);
		if (msg != 0xeeeeeeee) {
			chksum = 0;
			for (i = 1; i <= shm[0]; i++) {
				chksum += shm[i];
			}
			PRINTF("RECEIVING TASK: CHECKSUM = %x\n", chksum);
			if (chksum != shm[shm[0] + 1]) {
				PRINTF("RECEIVING TASK: WRONG SHMEM CHECKSUM\n");
			}
			PRINTF("RECEIVING TASK SENDS TO AGENT TASK %x\n", 0xaaaaaaaa);
			rt_send(agentask, 0xaaaaaaaa);
		} else {
			PRINTF("RECEIVING TASK DELETES ITSELF\n");
			rt_task_delete(receiving_task);
			PRINTF("END RECEIVING TASK\n");
			exit(1);
		}
		t = rt_get_cpu_time_ns();
		rt_named_hfree(inloop1);
		rt_free(inloop2);
		PRINTF("RECEIVER FREE TIME %d\n", (int)(rt_get_cpu_time_ns() - t));
	}
	rt_make_soft_real_time();
	PRINTF("RECEIVER ENDS\n");
	rt_named_hfree(shm);
	PRINTF("RECEIVER ENDS\n");
	rt_global_heap_close();
	PRINTF("RECEIVER ENDS\n");
	rt_heap_close(nam2num("HEAP"), heap);
	PRINTF("RECEIVER ENDS %d\n", getpid());
	rt_make_soft_real_time();
	rt_task_delete(receiving_task);
	return 0;
}
