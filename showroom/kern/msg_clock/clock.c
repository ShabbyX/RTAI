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


/* MODULE CLOCK */
#include <linux/module.h>
#include <linux/ctype.h>

#include <asm/io.h>

#include <asm/rtai.h>
#include <rtai_sched.h>
#include <rtai_sem.h>
#include <rtai_msg.h>
#include <rtai_fifos.h>

#include "clock.h"

MODULE_LICENSE("GPL");

#define Keyboard 0
#define Screen   1

#define TICK_PERIOD    100000LL    /*  0.1 msec (  1  tick) */
#define POLLING_DELAY  1000000LL    /*    1 msec ( 10 ticks) */
#define ONE_UNIT      10000000LL    /*   10 msec (100 ticks) */
#define FIVE_SECONDS  5000000000LL

#define TIMER_TO_CPU 3 // < 0 || > 1 to maintain a symmetric processed timer.

#define CLOCK_ON_CPUS  3  // 1: on cpu 0 only, 2: on cpu 1 only, 3: on any.
#define CHRONO_ON_CPUS 3  // 1: on cpu 0 only, 2: on cpu 1 only, 3: on any.
#define READ_ON_CPUS   3  // 1: on cpu 0 only, 2: on cpu 1 only, 3: on any.
#define WRITE_ON_CPUS  3  // 1: on cpu 0 only, 2: on cpu 1 only, 3: on any.

#define CLOCK_RUN_ON_CPUS  (num_online_cpus() > 1 ? CLOCK_ON_CPUS  : 1)
#define CHRONO_RUN_ON_CPUS (num_online_cpus() > 1 ? CHRONO_ON_CPUS : 1)
#define READ_RUN_ON_CPUS   (num_online_cpus() > 1 ? READ_ON_CPUS   : 1)
#define WRITE_RUN_ON_CPUS  (num_online_cpus() > 1 ? WRITE_ON_CPUS  : 1)

static SEM keybrd_sem;

static BOOLEAN hide;
static BOOLEAN pause;

extern int cpu_used[];

static RT_TASK read;
static RT_TASK clock;
static RT_TASK chrono;
static RT_TASK write;


static void rt_fractionated_sleep(RTIME OneUnit)
{
#define FRACT 100
	int i = FRACT;
	while (i--) {
		rt_sleep(llimd(OneUnit, 1, FRACT));
	}
}


static int keybrd_handler(unsigned int fifo)
{
	rt_sem_signal(&keybrd_sem);
	return 0;
}


static void ClockChrono_Read(long t)
{
	char ch;
	unsigned int run = 0;

	while(1) {
		cpu_used[hard_cpu_id()]++;
		rt_sem_wait(&keybrd_sem);
		rtf_get(Keyboard, &ch, 1);
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
				pause = TRUE;
				rt_fractionated_sleep(nano2count(FIVE_SECONDS));
				pause = FALSE;
				break;
			case 'K': case 'D':
				run += ch;
				if (run == ('K' + 'D')) {
					rt_send(&clock, run);
					rt_send(&chrono, run);
				}
				break;
		}
	}
}

static void ClockChrono_Clock(long t)
{
	RTIME OneUnit = nano2count(ONE_UNIT);
	const int hundredthes = FALSE;
	MenageHmsh_tHour hour;
	MenageHmsh_tChain11 hourChain;
	char command;
	BOOLEAN display;
	unsigned long msg;

	rt_receive(&read, &msg);
	MenageHmsh_Initialise(&hour);
	MenageHmsh_Convert(hour, hundredthes, &hourChain);
	Display_PutHour(hourChain);
	while(1) {
		cpu_used[hard_cpu_id()]++;
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
			}
		if (display) {
			MenageHmsh_Convert(hour, hundredthes, &hourChain);
			Display_PutHour(hourChain);
		}
	}
}

static void ClockChrono_Chrono(long t)
{
	RTIME OneUnit = nano2count(ONE_UNIT);
	MenageHmsh_tHour times;			
	MenageHmsh_tChain11 timesChain;
	BOOLEAN Intermediatetimes = FALSE;
	MenageHmsh_tHour endIntermediateTimes;
	BOOLEAN display;
	BOOLEAN hundredthes = FALSE;
	char command;
	unsigned long msg;

	rt_receive(&read, &msg);
	command = 'R';
	while(1) {
		cpu_used[hard_cpu_id()]++;
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
		}
		if (display) {
			MenageHmsh_Convert(times, hundredthes, &timesChain);
			Display_PutTimes(timesChain);
		}
		CommandChrono_Get(&command);
	}
}

static void ClockChrono_Write(long t)
{
	Display_tDest receiver;
	MenageHmsh_tChain11 chain;
	char buf[25] = "00:00:00   00:00:00:00";
	int i;

	while(1) {
		cpu_used[hard_cpu_id()]++;
		Display_Get(&chain, &receiver);

		if (receiver == destChrono) {
			for (i = 0; i < 11; i++) {
				buf[i+11] = chain.chain[i+1];
			}
		} else {
			for (i = 0; i < 8; i++) {
				buf[i] = chain.chain[i+1];
			}
		}
/*
		printk("\r%s K ",buf);
*/
			if (!hide && !pause) {
				rtf_put(Screen, chain.chain, 12);
			}
	}
}

int init_module(void)
{
	hide = FALSE;
	pause = FALSE;
	rtf_create(Keyboard, 1000);
	rtf_create_handler(Keyboard, keybrd_handler);
	rtf_create(Screen, 10000);
	rt_sem_init(&keybrd_sem, 0);
	rt_task_init(&read, ClockChrono_Read, 0, 2000, 0, 0, 0);
	rt_set_runnable_on_cpus(&read, READ_RUN_ON_CPUS);
	rt_task_init(&chrono, ClockChrono_Chrono, 0, 2000, 0, 0, 0);
	rt_set_runnable_on_cpus(&chrono, CHRONO_RUN_ON_CPUS);
	rt_task_init(&clock, ClockChrono_Clock, 0, 2000, 0, 0, 0);
	rt_set_runnable_on_cpus(&clock, CLOCK_RUN_ON_CPUS);
	rt_task_init(&write, ClockChrono_Write, 0, 2000, 0, 0, 0);
	rt_set_runnable_on_cpus(&write, WRITE_RUN_ON_CPUS);
	rt_assign_irq_to_cpu(TIMER_8254_IRQ, TIMER_TO_CPU);
	start_rt_timer((int)nano2count(TICK_PERIOD));
	rt_task_resume(&read);
	rt_task_resume(&chrono);
	rt_task_resume(&clock);
	rt_task_resume(&write);
	return 0;
}

void cleanup_module(void)
{
	int cpuid;
	rt_reset_irq_to_sym_mode(TIMER_8254_IRQ);
	stop_rt_timer();
	rt_busy_sleep(10000000);
	rt_task_delete(&read);
	rt_task_delete(&chrono);
	rt_task_delete(&clock);
	rt_task_delete(&write);
	rtf_destroy(Keyboard);
	rtf_destroy(Screen);
	printk("\n\nCPU USE SUMMARY\n");
	for (cpuid = 0; cpuid < NR_RT_CPUS; cpuid++) {
		printk("# %d -> %d\n", cpuid, cpu_used[cpuid]);
	}
	printk("END OF CPU USE SUMMARY\n\n");
}
