/*
COPYRIGHT (C) 1999  Paolo Mantegazza <mantegazza@aero.polimi.it>

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

#define MSG_DELAY 1000000000

int main(int argc, char* argv[])
{
	unsigned long btsk_name = nam2num("BTSK");
	unsigned long sem_name  = nam2num("SEM");
	unsigned long smbx_name  = nam2num("SMBX");
	unsigned long rmbx_name  = nam2num("RMBX");
	unsigned long msg;

	RT_TASK *mtsk, *btsk;
	SEM *sem;
	MBX *smbx, *rmbx;
	int count;

	long long mbx_msg;
	long long llmsg = 0xbbbbbbbbbbbbbbbbLL;

 	if (!(btsk = rt_task_init_schmod(nam2num("BTSK"), 0, 0, 0, SCHED_FIFO, 0x1))) {
		printf("CANNOT INIT BUDDY TASK\n");
		exit(1);
	}
	rt_make_hard_real_time();
	printf("BUDDY TASK: name = %lx, address = %p.\n", btsk_name, btsk);
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_sleep(nano2count(1000000000));

 	if (!(mtsk = rt_get_adr(nam2num("MTSK")))) {
		printf("CANNOT FIND MASTER TASK\n");
		exit(1);
	}

	printf("BUDDY TASK RESUMES MASTER TASK\n");
	rt_task_resume(mtsk);
	printf("BUDDY TASK RESUMED MASTER TASK\n");

	printf("BUDDY TASK SLEEP/LOOPS POLLING FOR SEM CREATED BY MASTER TASK\n");
 	while (!(sem = rt_get_adr(sem_name))) {
		rt_sleep(nano2count(1000000000));
	}
	printf("BUDDY TASK FOUND SEM, SLEEPS 1 s\n");
	rt_sleep(nano2count(1000000000));

	printf("BUDDY TASK SIGNALS MASTER TASK THROUGH SEM\n");
	rt_sem_signal(sem);

	printf("BUDDY TASK SLEEP/LOOPS POLLING FOR SEM DELETE BY MASTER TASK\n");
 	while ((sem = rt_get_adr(sem_name))) {
		rt_sleep(nano2count(1000000000));
	}

	printf("BUDDY TASK SENDS MESSAGE %lx TO MASTER TASK\n", 0xaaaaaaaaL);
	rt_send(mtsk, 0xaaaaaaaa);

	printf("BUDDY TASK WAITS TO RECEIVE FROM MASTER TASK\n");
	rt_receive(mtsk, (void *)&msg);
	printf("BUDDY TASK RECEIVED MESSAGE %lx FROM MASTER TASK AND RETURNS %lx\n", msg, 0xbbbbbbbbL);
	rt_return(mtsk, 0xbbbbbbbb);
	printf("BUDDY TASK SLEEP/LOOPS POLLING FOR MAILBOXES CREATED BY MASTER TASK\n");
 	while (!(smbx = rt_get_adr(smbx_name)) || !(rmbx = rt_get_adr(rmbx_name))) {
		rt_sleep(nano2count(1000000000));
	}
	printf("BUDDY TASK FOUND MAILBOXES, WAITS FOR A MESSAGE %p %p %p %p\n", smbx, rmbx, &btsk_name, &msg);

	count = 0;
	while(!rt_mbx_receive_timed(smbx, &mbx_msg, sizeof(mbx_msg), nano2count(MSG_DELAY))) {
		printf("%d BUDDY TASK RECEIVED %llx FROM MBX\n", count++, mbx_msg);

		printf("BUDDY TASK SENDS %llx TO MBX\n", llmsg);
		rt_mbx_send_timed(rmbx, &llmsg, sizeof(llmsg), nano2count(MSG_DELAY));
	}

	printf("BUDDY TASK WAITS A MESSAGE TO END SELF\n");
	rt_receive(mtsk, (void *)&msg);
	printf("BUDDY TASK RECEIVED MESSAGE TO END SELF %lx\n", msg);

	printf("BUDDY TASK DELETES ITSELF\n");
	rt_task_delete(btsk);

	printf("END BUDDY TASK\n");

	return 0;
}
