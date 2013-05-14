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
#include <unistd.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <rtai_mbx.h>
#include <rtai_msg.h>

#define PERIODIC_LOOPS 1000

#define SLEEP_LOOPS 1000

#define MBX_LOOPS 3000

#define PERIOD 50000

#define DELAY 50000

#define MSG_DELAY 1000000000

int main(int argc, char* argv[])
{
	unsigned long mtsk_name = nam2num("MTSK");
	unsigned long btsk_name = nam2num("BTSK");
	unsigned long sem_name  = nam2num("SEM");
	unsigned long smbx_name  = nam2num("SMBX");
	unsigned long rmbx_name  = nam2num("RMBX");
	unsigned long msg;

	long long mbx_msg;
	long long llmsg = 0xaaaaaaaaaaaaaaaaLL;

	RT_TASK *mtsk, *rcvd_from;
	SEM *sem;
	MBX *smbx, *rmbx;
	int pid, count;

 	if (!(mtsk = rt_task_init_schmod(mtsk_name, 0, 0, 0, SCHED_FIFO, 0x1))) {
		printf("CANNOT INIT MASTER TASK\n");
		exit(1);
	}
	printf("MASTER TASK INIT: name = %lx, address = %p.\n", mtsk_name, mtsk);

	printf("MASTER TASK STARTS THE ONESHOT TIMER\n");
	rt_set_oneshot_mode();
	start_rt_timer(nano2count(10000000));
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();
	rt_sleep(1000000);


	printf("MASTER TASK MAKES ITSELF PERIODIC WITH A PERIOD OF 1 ms\n");
	rt_task_make_periodic(mtsk, rt_get_time(), nano2count(PERIOD));
	rt_sleep(nano2count(1000000000));

	count = PERIODIC_LOOPS;
	printf("MASTER TASK LOOPS ON WAIT_PERIOD FOR %d PERIODS\n", count);
	while(count--) {
		printf("PERIOD %d\n", count);
		rt_task_wait_period();
	}

	count = SLEEP_LOOPS;
	printf("MASTER TASK LOOPS ON SLEEP 0.1 s FOR %d PERIODS\n", count);
	while(count--) {
		printf("SLEEPING %d\n", count);
		rt_sleep(nano2count(DELAY));
	}
	printf("MASTER TASK YIELDS ITSELF\n");
	rt_task_yield();

	printf("MASTER TASK CREATES BUDDY TASK\n");
	pid = fork();
	if (!pid) {
		execl("./slave", "./slave", NULL);
	}

	printf("MASTER TASK SUSPENDS ITSELF, TO BE RESUMED BY BUDDY TASK\n");
	rt_task_suspend(mtsk);
	printf("MASTER TASK RESUMED BY BUDDY TASK\n");

 	if (!(sem = rt_sem_init(sem_name, 0))) {
		printf("CANNOT CREATE SEMAPHORE %lx\n", sem_name);
		exit(1);
	}
	printf("MASTER TASK CREATES SEM: name = %lx, address = %p.\n", sem_name, sem);

	printf("MASTER TASK WAIT_IF ON SEM\n");
	rt_sem_wait_if(sem);

	printf("MASTER STEP BLOCKS WAITING ON SEM\n");
	rt_sem_wait(sem);

	printf("MASTER TASK SIGNALLED BY BUDDY TASK WAKES UP AND BLOCKS WAIT TIMED 1 s ON SEM\n");
	rt_sem_wait_timed(sem, nano2count(1000000000));

	printf("MASTER TASK DELETES SEM\n");
	rt_sem_delete(sem);

	printf("MASTER TASK BLOCKS RECEIVING FROM ANY\n");
	rcvd_from = rt_receive(0, (void *)&msg);
	printf("MASTER TASK RECEIVED MESSAGE %lx FROM BUDDY TASK\n", msg);

	printf("MASTER TASK RPCS TO BUDDY TASK THE MESSAGE %lx\n", 0xabcdefL);
	rcvd_from = rt_rpc(rcvd_from, 0xabcdef, (void *)&msg);
	printf("MASTER TASK RECEIVED THE MESSAGE %lx RETURNED BY BUDDY TASK\n", msg);
//exit(1);
 	if (!(smbx = rt_mbx_init(smbx_name, 1))) {
		printf("CANNOT CREATE MAILBOX %lx\n", smbx_name);
		exit(1);
	}
 	if (!(rmbx = rt_mbx_init(rmbx_name, 1))) {
		printf("CANNOT CREATE MAILBOX %lx\n", rmbx_name);
		exit(1);
	}
	printf("MASTER TASK CREATED TWO MAILBOXES %p %p %p %p \n", smbx, rmbx, &mtsk_name, &msg);

	count = MBX_LOOPS;
	while(count--) {
		rt_mbx_send(smbx, &llmsg, sizeof(llmsg));
		printf("%d MASTER TASK SENDS THE MESSAGE %llx MBX\n", count, llmsg);
		mbx_msg = 0;
		rt_mbx_receive_timed(rmbx, &mbx_msg, sizeof(mbx_msg), nano2count(MSG_DELAY));
		printf("%d MASTER TASK RECEIVED THE MESSAGE %llx FROM MBX\n", count, mbx_msg);
		rt_sleep(nano2count(DELAY));
	}

	printf("MASTER TASK SENDS THE MESSAGE %lx TO BUDDY TO ALLOW ITS END\n", 0xeeeeeeeeL);
	rt_send(rcvd_from, 0xeeeeeeee);

	printf("MASTER TASK WAITS FOR BUDDY TASK END\n");
	while (rt_get_adr(btsk_name)) {
		rt_sleep(nano2count(1000000000));
	}
	printf("MASTER TASK STOPS THE PERIODIC TIMER\n");
	stop_rt_timer();

	printf("MASTER TASK DELETES MAILBOX %p\n", smbx);
	rt_mbx_delete(smbx);
	printf("MASTER TASK DELETES MAILBOX %p\n", rmbx);
	rt_mbx_delete(rmbx);

	printf("MASTER TASK DELETES ITSELF\n");
	rt_task_delete(mtsk);

	printf("END MASTER TASK\n");

	return 0;
}
