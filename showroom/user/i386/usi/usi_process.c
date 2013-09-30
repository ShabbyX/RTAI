/*
COPYRIGHT (C) 2002-2008  Paolo Mantegazza <mantegazza@aero.polimi.it>

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
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include <rtai_lxrt.h>
#include <rtai_sem.h>
#include <rtai_usi.h>

#include <rtc.c>

#define TIMER_IRQ  RTC_IRQ
#define TIMER_FRQ  1024
#define TIMEOUT    100000000  // to watch, or not to watch, overruns (MP only)

static SEM *dspsem;
static volatile int ovr, intcnt;

#include "check_flags.h"

static void *timer_handler(void *args)
{
	RT_TASK *handler;

 	if (!(handler = rt_task_init_schmod(nam2num("HANDLR"), 0, 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT HANDLER TASK > HANDLR <\n");
		exit(1);
	}
	rt_allow_nonroot_hrt();
	mlockall(MCL_CURRENT | MCL_FUTURE);
	rt_make_hard_real_time();

	rt_request_irq_task(TIMER_IRQ, handler, RT_IRQ_TASK, 1);
	rtc_start(TIMER_FRQ);
	rtc_enable_irq(TIMER_IRQ, TIMER_FRQ);
	rtai_cli();
	while (ovr != RT_IRQ_TASK_ERR) {
		CHECK_FLAGS();
		do {
			ovr = rt_irq_wait_timed(TIMER_IRQ, nano2count(TIMEOUT));
			if (ovr == RT_IRQ_TASK_ERR) break;
			if (ovr > 0) {
				/* overrun processing, if any, goes here */
				rt_sem_signal(dspsem);
			}
			/* normal processing goes here */
			intcnt++;
			rt_sem_signal(dspsem);
		} while (ovr > 0);
		rtc_enable_irq(TIMER_IRQ, TIMER_FRQ);
	}
	rtai_sti();
	rtc_stop();
	rt_release_irq_task(TIMER_IRQ);
	rt_make_soft_real_time();
	rt_task_delete(handler);
	return 0;
}

int main(void)
{
	RT_TASK *maint;
	pthread_t thread;
	int maxcnt;

	printf("GIVE THE NUMBER OF INTERRUPTS YOU WANT TO COUNT: ");
	scanf("%d", &maxcnt);

	start_rt_timer(0);
	if (!(maint = rt_task_init(nam2num("MAIN"), 1, 0, 0))) {
		printf("CANNOT INIT MAIN TASK > MAIN <\n");
		exit(1);
	}

	if (!(dspsem = rt_sem_init(nam2num("DSPSEM"), 0))) {
		printf("CANNOT INIT SEMAPHORE > DSPSEM <\n");
		exit(1);
	}
	thread = rt_thread_create(timer_handler, NULL, 10000);
	while (intcnt < maxcnt) {
		rt_sem_wait(dspsem);
		printf("OVERRUNS %d, INTERRUPT COUNT %d\n", ovr, intcnt);
	}
	rtc_stop();
	rt_release_irq_task(TIMER_IRQ);
	rt_thread_join(thread);
	rt_task_delete(maint);
	rt_sem_delete(dspsem);
	printf("TEST ENDS\n");
	return 0;
}
