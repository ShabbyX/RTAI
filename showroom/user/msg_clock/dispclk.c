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
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <rtai_msg.h>

#include "clock.h"

void MenageHmsh_Initialise(MenageHmsh_tHour *h)
{
	h->hours = h->minutes = h->seconds = h->hundredthes = 0;
}

void MenageHmsh_InitialiseHundredthes(MenageHmsh_tHour *h)
{
	h->hundredthes = 0;
}

BOOLEAN MenageHmsh_Equal(MenageHmsh_tHour h1, MenageHmsh_tHour h2)
{
	return 	   (h1.hours == h2.hours)
		&& (h1.minutes == h2.minutes)
		&& (h1.seconds == h2.seconds)
		&& (h1.hundredthes == h2.hundredthes);
}

void MenageHmsh_AdvanceHours(MenageHmsh_tHour *h)
{
	h->hours = (h->hours + 1)%24;
}

void MenageHmsh_AdvanceMinutes(MenageHmsh_tHour *h)
{
	h->minutes = (h->minutes + 1)%60;
}

void MenageHmsh_AdvanceSeconds(MenageHmsh_tHour *h)
{
	h->seconds = (h->seconds + 1)%60;
}

void MenageHmsh_PlusOneSecond(MenageHmsh_tHour *h)
{
	MenageHmsh_AdvanceSeconds(h);
	if (h->seconds == 0) {
		MenageHmsh_AdvanceMinutes(h);
		if (h->minutes == 0) {
			MenageHmsh_AdvanceHours(h);
		}
	}
}

void MenageHmsh_PlusNSeconds(int n, MenageHmsh_tHour *h)
{
	int i;
	for (i = 1; i <= n; i++) {
		MenageHmsh_PlusOneSecond(h);
	}
}

void MenageHmsh_PlusOneUnit(MenageHmsh_tHour *h, BOOLEAN *reportSeconds)
{
	h->hundredthes = (h->hundredthes + 1)%100;
	*reportSeconds = h->hundredthes == 0;
	if (*reportSeconds) {
		MenageHmsh_PlusOneSecond(h);
	}
}

void MenageHmsh_Convert(MenageHmsh_tHour h, BOOLEAN hundredthes, MenageHmsh_tChain11 *chain)
{
	static char digits[10] = {'0','1','2','3','4','5','6','7','8','9'};
	chain->chain[1] = digits[h.hours/10];
	chain->chain[2] = digits[h.hours%10];
	chain->chain[3] = ':';
	chain->chain[4] = digits[h.minutes/10];
	chain->chain[5] = digits[h.minutes%10];
	chain->chain[6] = ':';
	chain->chain[7] = digits[h.seconds/10];
	chain->chain[8] = digits[h.seconds%10];
	if (hundredthes) {
		chain->chain[9] = '.';
		chain->chain[10] = digits[h.hundredthes/10];
		chain->chain[11] = digits[h.hundredthes%10];
	} else {
		chain->chain[9] = chain->chain[10] = chain->chain[11] = ' ';
	}
}

void Display_PutHour(MenageHmsh_tChain11 chain)
{
	static MenageHmsh_tChain11 hours;
	static RT_TASK *ackn = 0;
	unsigned int put = 'p';
	unsigned long msg;

	if (ackn != rt_get_adr(nam2num("DSPTSK"))) {
		ackn = rt_rpc(rt_get_adr(nam2num("DSPTSK")), put, &msg);
	}
	hours = chain;
	hours.chain[0] = 'h';
	rt_send_if(rt_get_adr(nam2num("DSPTSK")), (unsigned long)hours.chain);
}

void Display_PutTimes(MenageHmsh_tChain11 chain)
{
	static MenageHmsh_tChain11 times;
	static RT_TASK *ackn = 0;
	unsigned int put = 'P';
	unsigned long msg;

	if (ackn != rt_get_adr(nam2num("DSPTSK"))) {
		ackn = rt_rpc(rt_get_adr(nam2num("DSPTSK")), put, &msg);
	}
	times = chain;
	times.chain[0] = 't';
	rt_send_if(rt_get_adr(nam2num("DSPTSK")), (unsigned long)times.chain);
}

void Display_Get(MenageHmsh_tChain11 *chain, Display_tDest *receiver)
{
	static RT_TASK *ackn = 0;
	unsigned int get = 'g';
	unsigned long msg;

	if (ackn != rt_get_adr(nam2num("DSPTSK"))) {
		ackn = rt_rpc(rt_get_adr(nam2num("DSPTSK")), get, &msg);
	}
	rt_receive(rt_get_adr(nam2num("DSPTSK")), &msg);
	*chain = *((MenageHmsh_tChain11 *)msg);
	*receiver = chain->chain[0] == 't' ? destChrono : destClock;
}

void *Display_task(void *args)
{
	RT_TASK *mytask;
	unsigned long command;
	int ackn = 0;
	RT_TASK *get = (RT_TASK *)0, *tput = (RT_TASK *)0, *hput = (RT_TASK *)0, *task;

 	if (!(mytask = rt_thread_init(nam2num("DSPTSK"), 1, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT TASK Display_task\n");
		exit(1);
	}
	printf("INIT TASK Display_task %p.\n", mytask);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	while (ackn != ('g' + 'p' + 'P')) {
		task = rt_receive((RT_TASK *)0, &command);
		switch (command) {
			case 'g':
				get = task;
				ackn += command;
				break;
			case 'p':
				tput = task;
				ackn += command;
				break;
			case 'P':
				hput = task;
				ackn += command;
				break;
		}
	}
	rt_return(get, command);
	rt_return(tput, command);
	rt_return(hput, command);

	while(1) {
		task = rt_receive(0, &command);
		if (task == tput || task == hput) {
			rt_send(get, command);
			if (((char *)command)[1] == 101) {
				goto end;
			}
		}
	}
end:
	rt_task_delete(mytask);
	printf("END TASK Display_task %p.\n", mytask);
	return 0;
}
