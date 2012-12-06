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


/* MODULE DISPCLK */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <asm/system.h>
#include <asm/fixmap.h>
#include <asm/smp.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/atomic.h>
#include <asm/desc.h>

#include <asm/rtai.h>
#include <rtai_sched.h>
#include <rtai_sem.h>
#include <rtai_fifos.h>

#include "clock.h"

static char digits[10] = {'0','1','2','3','4','5','6','7','8','9'};

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

static enum {empty, full} HourBufferstatus, TimesBufferstatus;
static MenageHmsh_tChain11 hour;
static MenageHmsh_tChain11 times;
static SEM notEmpty, mutex;

void Display_PutHour(MenageHmsh_tChain11 chain)
{
	rt_sem_wait(&mutex);
	hour = chain;
	hour.chain[0] = 'h';
	if (HourBufferstatus == empty) {
		HourBufferstatus = full;
		rt_sem_signal(&notEmpty);
	}
	rt_sem_signal(&mutex);
}

void Display_PutTimes(MenageHmsh_tChain11 chain)
{
	rt_sem_wait(&mutex);
	times = chain;
	times.chain[0] = 't';
	if (TimesBufferstatus == empty) {
		TimesBufferstatus = full;
		rt_sem_signal(&notEmpty);
	}
	rt_sem_signal(&mutex);
}

void Display_Get(MenageHmsh_tChain11 *chain, Display_tDest *receiver)
{
	rt_sem_wait(&notEmpty);
	rt_sem_wait(&mutex);
	if (TimesBufferstatus == full) {
		*receiver = destChrono; 
		*chain = times;
		TimesBufferstatus = empty;
	} else if (HourBufferstatus == full) {
		*receiver = destClock; 
		*chain = hour;
		HourBufferstatus = empty;
	}
	rt_sem_signal(&mutex);
}

int init_module(void)
{
	rt_sem_init(&notEmpty, 0);
	rt_sem_init(&mutex, 1);
	TimesBufferstatus = empty;
	HourBufferstatus = empty;
	return 0;
}

void cleanup_module(void)
{
}

EXPORT_SYMBOL(MenageHmsh_AdvanceSeconds);
EXPORT_SYMBOL(MenageHmsh_PlusOneUnit);
EXPORT_SYMBOL(MenageHmsh_InitialiseHundredthes);
EXPORT_SYMBOL(MenageHmsh_AdvanceHours);
EXPORT_SYMBOL(MenageHmsh_PlusNSeconds);
EXPORT_SYMBOL(MenageHmsh_Equal);
EXPORT_SYMBOL(MenageHmsh_AdvanceMinutes);
EXPORT_SYMBOL(MenageHmsh_Initialise);
EXPORT_SYMBOL(MenageHmsh_Convert);
EXPORT_SYMBOL(Display_PutTimes);
EXPORT_SYMBOL(Display_Get);
EXPORT_SYMBOL(Display_PutHour);
