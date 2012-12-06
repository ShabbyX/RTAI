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


#include <asm/io.h>
#include <rtai_registry.h>
#include <rtai_msg.h>
#include <rtai_shm.h>
#include <rtai_sem.h>

//#define VERIFY_FLAGS
#ifdef  VERIFY_FLAGS
#define CHECK_FLAGS \
	{ \
		unsigned long flags; \
		rt_global_save_flags(&flags); \
		if (flags != ((1 << IFLAG) | 1)) { \
			rt_printk("FLAGS: %lx\n", flags); \
		} \
	}
#else
#define CHECK_FLAGS
#endif

//#define SUPRT USE_GFP_ATOMIC
#define SUPRT USE_VMALLOC

#define TICK_PERIOD 100000

#define STACK_SIZE 2000

#define SHMSIZE 4000

static int cpu_used[NR_RT_CPUS];

RT_TASK agentask;

int *shm;

SEM agentsem;
SEM shmsem;

void fun(long t)
{
	int op;
	RT_TASK *msgtsk;
	unsigned long i, chksum, msg;

	while (!rt_get_adr(nam2num("STSK")) || !rt_get_adr(nam2num("RTSK"))) {
		cpu_used[hard_cpu_id()]++;
		CHECK_FLAGS;
		rt_task_wait_period();
	}
	op = 4;

	while(shm[0]) {
		cpu_used[hard_cpu_id()]++;
		CHECK_FLAGS;
		/* here do any periodic real time job, then act as agent*/
		switch (op) {
			case 1: {
				if(!rt_sem_wait_if(&agentsem)) {
//					printk("AGENT TASK NOT SIGNALLED YET ON AGENTSEM\n");
					break;
				}
//				printk("AGENT TASK SIGNALLED ON AGENTSEM\n");
				op = 2;
				chksum = 0;
				for (i = 1; i <= shm[0]; i++) {
					chksum += shm[i];
				}
//				printk("AGENT TASK: CHECKSUM = %x\n", chksum);
				if (chksum != shm[shm[0] + 1]) {
					rt_printk("\n\n\nATSK: WRONG CHECKSUM\n\n\n");
				}
			}
			case 2: {
				if (!(msgtsk = rt_receive_if(0, &msg))) {
//					printk("AGENT TASK NOT RPCED YET\n");
					break;
				}
//				printk("AGENT TASK RPCED WITH %x\n", msg);
				op = 3;
//				printk("AGENT TASK RETURNS %x\n", msg);
				rt_return(msgtsk, msg);
			}
			case 3: {
				if (!rt_receive_if(0, &msg)) {
//					printk("AGENT NOT SENT YET\n");
					break;
				}
			}
			case 4: {
				op = 1;
//				printk("AGENT TASK SENT WITH %x, SIGNALS SHMSEM\n", msg);
				rt_sem_signal(&shmsem);
				break;
			}
		}
		rt_task_wait_period();
	}
//	printk("AGENT TASK EXITED LOOP ON NULL SHM, WAITS TO BE RPCED\n");
	while(!(msgtsk = rt_receive_if(0, &msg))) {
		cpu_used[hard_cpu_id()]++;
		CHECK_FLAGS;
//		printk("AGENT TASK NOT RPCED YET\n");
		rt_task_wait_period();
	}
	rt_printk("AGENT TASK LAST RPCED WITH %x RETURNS %x\n", msg, 0xeeeeeeee);
	rt_return(msgtsk, 0xeeeeeeee);
	rt_printk("AGENT TASK ENDS\n");
}

int init_module(void)
{
	int period;
	
	if (!(shm = rt_shm_alloc(nam2num("MEM"), (SHMSIZE + 2)*sizeof(int), SUPRT))) {
		printk("ERROR IN SHM ALLOC\n");
		return 0;
	}
	shm[0] = SHMSIZE;
	rt_sem_init(&shmsem, 0);
	rt_sem_init(&agentsem, 0);
	rt_task_init(&agentask, fun, 0, STACK_SIZE, 0, 0, 0);
	rt_register(nam2num("SHSM"), &shmsem, IS_SEM, current);
	rt_register(nam2num("AGSM"), &agentsem, IS_SEM, 0);
	rt_register(nam2num("ATSK"), &agentask, IS_TASK, 0);
	rt_set_oneshot_mode();
	period = start_rt_timer(nano2count(TICK_PERIOD));
	rt_task_make_periodic(&agentask, rt_get_time() + period, period);
	return 0;
}

void cleanup_module(void)
{
	int cpuid;

	stop_rt_timer();
	rt_busy_sleep(nano2count(TICK_PERIOD));
	rt_drg_on_name(nam2num("SHSM"));
	rt_drg_on_name(nam2num("AGSM"));
	rt_drg_on_name(nam2num("ATSK"));
	rt_task_delete(&agentask);
//	rt_shm_free(nam2num("MEM"));
	printk("\n\nCPU USE SUMMARY\n");
	for (cpuid = 0; cpuid < NR_RT_CPUS; cpuid++) {
		printk("# %d -> %d\n", cpuid, cpu_used[cpuid]);
	}
	printk("END OF CPU USE SUMMARY\n\n");
}
