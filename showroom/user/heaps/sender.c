/*
COPYRIGHT (C) 2004  Paolo Mantegazza <mantegazza@aero.polimi.it>

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
#include <rtai_sem.h>

#define SUPRT  USE_VMALLOC

#define COUNT 1000

//#define DISPLAY
#ifdef  DISPLAY
#define PRINTF(fmt, args...)  rt_printk(fmt, ##args)
#else
#define PRINTF(fmt, args...)  printf(fmt, ##args)
#endif

int main(void)
{
	RT_TASK *sending_task ;
	SEM *shmsem, *agentsem;
	int i, *shm, shm_size, count;
	void *heap;
	unsigned int chksum;

	sending_task = rt_task_init_schmod(nam2num("STSK"), 0, 0, 0, SCHED_FIFO, 0xF);
	heap = rt_heap_open(nam2num("HEAP"), 0, SUPRT);
	rt_global_heap_open();
	shm = rt_named_halloc(nam2num("MEM"), 0);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();
	rt_halloc(9);
	rt_halloc(13);
	shm_size = shm[0];
printf("SHMSIZE %d\n", shm_size);
	shmsem   = rt_get_adr(nam2num("SHSM"));
	agentsem = rt_get_adr(nam2num("AGSM"));
	count = COUNT;
	rt_malloc(23);
	while(count--) {
		RTIME t;
		void *inloop1, *inloop2;
		t = rt_get_cpu_time_ns();
		inloop1 = rt_named_halloc(nam2num("piopio"), count);
		inloop2 = rt_malloc(count + 3);
		PRINTF("SENDER ALLOC TIME %d %d\n", (int)(rt_get_cpu_time_ns() - t), count + count + 3);
		PRINTF("SENDING TASK WAIT ON SHMSEM\n");
		rt_sem_wait(shmsem);
		PRINTF("SENDING TASK SIGNALLED ON SHMSEM\n");
			if (!(shm[0] = ((float)rand()/(float)RAND_MAX)*shm_size) || shm[0] > shm_size) {
				shm[0] = shm_size;
			}
			chksum = 0;
			for (i = 1; i <= shm[0]; i++) {
				shm[i] = rand();
				chksum += shm[i];
			}
			shm[shm[0] + 1] = chksum;
			PRINTF("STSK: %d CHECKSUM = %x\n", count, chksum);
		PRINTF("SENDING TASK SIGNAL AGENTSEM\n");
		rt_sem_signal(agentsem);
		t = rt_get_cpu_time_ns();
		rt_named_hfree(inloop1);
		rt_free(inloop2);
		PRINTF("SENDER FREE TIME %d\n", (int)(rt_get_cpu_time_ns() - t));
	}
	PRINTF("SENDING TASK LAST WAIT ON SHMSEM\n");
	rt_sem_wait(shmsem);
	PRINTF("SENDING TASK SIGNALLED ON SHMSEM\n");
	shm[0] = 0;
	PRINTF("SENDING TASK LAST SIGNAL TO AGENTSEM\n");
	rt_sem_signal(agentsem);
	rt_named_hfree(shm);
	rt_heap_close(nam2num("HEAP"), heap);
	rt_global_heap_close();
	printf("SENDING TASK DELETES ITSELF\n");
	rt_make_soft_real_time();
	rt_task_delete(sending_task);
	printf("END SENDING TASK %d\n", getpid());
	return 0;
}
