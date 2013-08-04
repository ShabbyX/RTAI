/*
COPYRIGHT (C) 2006  Paolo Mantegazza <mantegazza@aero.polimi.it>

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
#include <ctype.h>

#include <semaphore.h>
#include <rtai_pmq.h>

#include "clock.h"

#define TICK_PERIOD     100000LL    /*  0.1 msec (  1  tick) */
#define POLLING_DELAY  1000000LL    /*    1 msec ( 10 ticks) */
#define ONE_UNIT      10000000LL    /*   10 msec (100 ticks) */
#define FIVE_SECONDS  5000000000LL

static sem_t sync_sem;

static BOOLEAN hide;
static BOOLEAN Pause;

static RTIME OneUnit;

static void rt_fractionated_sleep(RTIME OneUnit)
{
#define FRACT 100
	int i = FRACT;
	while (i--) {
		rt_sleep(OneUnit/FRACT);
	}
}

void *ClockChrono_Read(void *args)
{
	RT_TASK *mytask;
	char ch;
	int run = 0;
	mqd_t Keyboard;
	struct mq_attr kb_attrs = { MAX_MSGS, 1, 0, 0 };

 	if (!(mytask = rt_thread_init(nam2num("READ"), 1, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT TASK ClockChronoRead\n");
		exit(1);
	}

	Keyboard = mq_open("KEYBRD", O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, &kb_attrs);
	printf("INIT TASK ClockChronoRead %p.\n", mytask);

	mlockall(MCL_CURRENT | MCL_FUTURE);

	while(1) {
		mq_receive(Keyboard, &ch, 1, NULL);
		ch = toupper(ch);
		switch(ch) {
			case 'T': case 'R': case 'H': case 'M': case 'S':
				CommandClock_Put(ch);
				break;
			case 'C': case 'I': case 'E':
				CommandChrono_Put(ch);
				break;
			case 'N':
				hide = ~hide;
				break;
			case 'P':
				Pause = TRUE;
				rt_fractionated_sleep(nano2count(FIVE_SECONDS));
				Pause = FALSE;
				break;
			case 'K': case 'D':
				run |= ch;
				if (run == ('K' | 'D')) {
					sem_post(&sync_sem);
					sem_post(&sync_sem);
				}
				break;
			case 'F':
				CommandClock_Put('F');
				CommandChrono_Put('F');
				goto end;
		}
	}
end:
	mq_close(Keyboard);
	rt_task_delete(mytask);
	printf("END TASK ClockChronoRead %p.\n", mytask);
	return 0;
}

void *ClockChrono_Clock(void *args)
{
	RT_TASK *mytask;
	const int hundredthes = FALSE;
	MenageHmsh_tHour hour;
	MenageHmsh_tChain11 hourChain;
	char command;
	BOOLEAN display;

 	if (!(mytask = rt_thread_init(nam2num("CLOCK"), 1, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT TASK ClockChronoClock\n");
		exit(1);
	}
	printf("INIT TASK ClockChronoClock %p.\n", mytask);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	rt_make_hard_real_time();
	sem_wait(&sync_sem);
	MenageHmsh_Initialise(&hour);
	while(1) {
		CommandClock_Get(&command);
		switch(command) {
			case 'R':
				rt_fractionated_sleep(OneUnit);
				MenageHmsh_PlusOneUnit(&hour, &display);
				break;
			case 'T':
				MenageHmsh_InitialiseHundredthes(&hour);
				display = FALSE;
				break;
			case 'H':
				MenageHmsh_AdvanceHours(&hour);
				display = TRUE;
				break;
			case 'M':
				MenageHmsh_AdvanceMinutes(&hour);
				display = TRUE;
				break;
			case 'S':
				MenageHmsh_AdvanceSeconds(&hour);
				display = TRUE;
				break;
			case 'F':
				goto end;
			}
		if (display) {
			MenageHmsh_Convert(hour, hundredthes, &hourChain);
			Display_PutHour(hourChain);
		}
	}
end:
	rt_make_soft_real_time();
	hourChain.chain[1] = 'e';
	Display_PutHour(hourChain);
	rt_task_delete(mytask);
	printf("END TASK ClockChronoClock %p.\n", mytask);
	return 0;
}

void *ClockChrono_Chrono(void *args)
{
	RT_TASK *mytask /*, *keybrd */;
	MenageHmsh_tHour times;
	MenageHmsh_tChain11 timesChain;
	BOOLEAN Intermediatetimes = FALSE;
	MenageHmsh_tHour endIntermediateTimes;
	BOOLEAN display;
	BOOLEAN hundredthes = FALSE;
	char command;

 	if (!(mytask = rt_thread_init(nam2num("CHRONO"), 1, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT TASK ClockChronoChrono\n");
		exit(1);
	}
	printf("INIT TASK ClockChronoChrono %p.\n", mytask);
	mlockall(MCL_CURRENT | MCL_FUTURE);

	rt_make_hard_real_time();
	sem_wait(&sync_sem);
	command = 'R';
	while(1) {
		switch(command) {
			case 'R':
				MenageHmsh_Initialise(&times);
				display = TRUE;
				hundredthes = FALSE;
				Intermediatetimes = FALSE;
				break;
			case 'C':
				rt_fractionated_sleep(OneUnit);
				MenageHmsh_PlusOneUnit(&times, &display);
				if (Intermediatetimes) {
					Intermediatetimes = !MenageHmsh_Equal(
						times, endIntermediateTimes);
					display = !Intermediatetimes;
					hundredthes = FALSE;
				}
				break;
			case 'I':
				Intermediatetimes = TRUE;
				endIntermediateTimes = times;
				MenageHmsh_PlusNSeconds(3,
							&endIntermediateTimes);
				display = TRUE;
				hundredthes = TRUE;
				break;
			case 'E':
				display = TRUE;
				hundredthes = TRUE;
				break;
			case 'F':
				goto end;
		}
		if (display) {
			MenageHmsh_Convert(times, hundredthes, &timesChain);
			Display_PutTimes(timesChain);
		}
		CommandChrono_Get(&command);
	}
end:
	rt_make_soft_real_time();
 	rt_task_delete(mytask);
	printf("END TASK ClockChronoChrono %p.\n", mytask);
	return 0;
}

void *ClockChrono_Write(void *args)
{
	RT_TASK *mytask;
	Display_tDest receiver;
	MenageHmsh_tChain11 chain;
	mqd_t Screen;
	struct mq_attr sc_attrs = { MAX_MSGS, 12, 0, 0 };

 	if (!(mytask = rt_thread_init(nam2num("WRITE"), 1, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT TASK ClockChronoWrite\n");
		exit(1);
	}

	Screen = mq_open("SCREEN", O_WRONLY | O_CREAT | O_NONBLOCK, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, &sc_attrs);
	printf("INIT TASK ClockChronoWrite %p.\n", mytask);

	mlockall(MCL_CURRENT | MCL_FUTURE);

	while(1) {
		Display_Get(&chain, &receiver);
		if (chain.chain[1] == 'e') {
			goto end;
		}
		if (!hide && !Pause) {
			mq_send(Screen, chain.chain, 12, 0);
		}
	}
end:
	mq_close(Screen);
 	rt_task_delete(mytask);
	printf("END TASK ClockChronoWrite %p.\n", mytask);
	return 0;
}

static int Read;
static int Clock;
static int Chrono;
static int Write;

int main(int argc, char* argv[])
{
	extern int init_cmdclk(void);
	extern int init_cmdcrn(void);
	extern int init_dispclk(void);
	extern int cleanup_cmdclk(void);
	extern int cleanup_cmdcrn(void);
	extern int cleanup_dispclk(void);
	RT_TASK *mytask;

	hide = FALSE;
	Pause = FALSE;
	sem_init(&sync_sem, 0, 0);

	rt_set_oneshot_mode();
	init_cmdclk();
	init_cmdcrn();
	init_dispclk();

 	if (!(mytask = rt_task_init(nam2num("MASTER"), 1, 0, 0))) {
		printf("CANNOT INIT MASTER TASK\n");
		exit(1);
	}
	printf("\nINIT MASTER TASK %p.\n", mytask);

	OneUnit = nano2count(ONE_UNIT);
	start_rt_timer((int)nano2count(TICK_PERIOD));

	if (!(Read = rt_thread_create(ClockChrono_Read, NULL, 10000))) {
		printf("ERROR IN CREATING ClockChrono_Read\n");
		exit(1);
 	}

	if (!(Chrono = rt_thread_create(ClockChrono_Chrono, NULL, 10000))) {
		printf("ERROR IN CREATING ClockChrono_Chrono\n");
		exit(1);
 	}

	if (!(Clock = rt_thread_create(ClockChrono_Clock, NULL, 10000))) {
		printf("ERROR IN CREATING ClockChrono_Clock\n");
		exit(1);
 	}

	if (!(Write = rt_thread_create(ClockChrono_Write, NULL, 10000))) {
		printf("ERROR IN CREATING ClockChrono_Write\n");
		exit(1);
 	}

	rt_task_suspend(mytask);

	while (rt_get_adr(nam2num("READ"))) {
		rt_sleep(nano2count(1000000));
	}
	while (rt_get_adr(nam2num("CLOCK"))) {
		rt_sleep(nano2count(1000000));
	}
	while (rt_get_adr(nam2num("CHRONO"))) {
		rt_sleep(nano2count(1000000));
	}
	while (rt_get_adr(nam2num("WRITE"))) {
		rt_sleep(nano2count(1000000));
	}
	cleanup_cmdclk();
	cleanup_cmdcrn();
	cleanup_dispclk();
	rt_thread_join(Chrono);
	rt_thread_join(Clock);
	rt_thread_join(Read);
	rt_thread_join(Write);
	sem_destroy(&sync_sem);
	rt_task_delete(mytask);
	printf("\nEND MASTER TASK %p.\n", mytask);
	return 0;
}
