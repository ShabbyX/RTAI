/*
COPYRIGHT (C) 2001-2006  Paolo Mantegazza  (mantegazza@aero.polimi.it)
                         Giuseppe Quaranta (quaranta@aero.polimi.it)
 
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include <linux/module.h>
#include <asm/io.h>
#include <linux/mc146818rtc.h>

MODULE_LICENSE("GPL");

#include <asm/rtai.h>
#include <rtai_schedcore.h>
#include <rtai_sched.h>
#include "sysAuxClk.h"

#define MIN_RTC_FREQ  2
#define MAX_RTC_FREQ  8192

static int enabled, ticks_per_sec = ERROR, pwr2 = ERROR;
static void *fun_task;

void rtc_intr_handler(void)
{
	if (enabled) {
#ifdef SINGLE_EXECUTION
		rt_task_resume(fun_task);
#else
		rt_send(fun_task, 0);
#endif
	}
 	CMOS_READ(RTC_INTR_FLAGS);
	rt_enable_irq(RTC_IRQ);
}

static void rtc_periodic_timer_setup(int pwr2)
{
	rt_disable_irq(RTC_IRQ);
	rt_release_irq(RTC_IRQ);
	rtai_hw_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
        CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	CMOS_WRITE(RTC_REF_CLCK_32KHZ | (16 - pwr2),          RTC_FREQ_SELECT);
	CMOS_WRITE((CMOS_READ(RTC_CONTROL) & 0x8F) | RTC_PIE, RTC_CONTROL);
	rtai_hw_sti();
	rt_request_irq(RTC_IRQ, (void *)rtc_intr_handler, 0, 0);
	rt_enable_irq(RTC_IRQ);
	CMOS_READ(RTC_INTR_FLAGS);
}

static RT_TASK timer_task;

static void timer_task_handler(int i)
{
	while (1) {
		if (enabled) {
#ifdef SINGLE_EXECUTION
			rt_task_resume(fun_task);
#else
			rt_send(fun_task, 0);
#endif
		}
		if (!fun_task) {
			break;
		}
		rt_task_wait_period();
	}
}
static void timer_task_setup(int tps)
{
	RTIME period;
	if (!timer_task.magic) {
		rt_task_init(&timer_task, (void *)timer_task_handler, 0, 8192, 0, 0, 0);
	}
	rt_set_periodic_mode();
	period = start_rt_timer(nano2count(1000000000/tps));
	rt_task_make_periodic(&timer_task, rt_get_time() + period, period);
}

static int UseTask = 1;
RTAI_MODULE_PARM(UseTask, int);

static void timer_setup(int tps)
{
	if (UseTask) {
		printk("***** USING A TASK BASED LOCAL TIMER *****\n");
		timer_task_setup(tps);
	} else {
		printk("***** USING THE RTC BASED LOCAL TIMER *****\n");
		rtc_periodic_timer_setup(tps);
	}
}

long long aux_clk_manager(unsigned long *args)
{
	switch (args[0]) {
		case AUX_CLK_RATE_SET: {
			int freq;
			if ((freq = args[1]) < MIN_RTC_FREQ) {
				return ticks_per_sec = pwr2 = ERROR;
			}
			if (!UseTask) {
				if (freq > MAX_RTC_FREQ || hweight32(freq) != 1) {
					return ticks_per_sec = pwr2 = ERROR;
				}
				pwr2 = ffs(freq) - 1;
			} 
			ticks_per_sec = UseTask ? freq : 1 << pwr2;
			return OK;
		}
		case AUX_CLK_RATE_GET: {
			return ticks_per_sec;
		}
		case AUX_CLK_CONNECT: {
			enabled = 0;
			fun_task = _rt_whoami();
			timer_setup(UseTask ? ticks_per_sec : pwr2);
			return ticks_per_sec;
		}
		case AUX_CLK_ENABLE: {
			if (fun_task) {
				enabled = 1;
			}
			return OK;
		}
		case AUX_CLK_DISABLE: {
			enabled = 0;
#ifndef SINGLE_EXECUTION
			rt_send(fun_task, 1);
#endif
			fun_task = NULL;
		}
	}
	return OK;
}

void donothing(void) { }

static int srq;

int init_module(void)
{
	if (!(srq = rt_request_srq(nam2num("AUXCLK"), donothing, (void *)aux_clk_manager))) {
		printk("CANNOT INSTALL AUX CLK\n");
		return 1;
	}
	return 0;
}

void cleanup_module(void)
{
	rt_disable_irq(RTC_IRQ);
	rt_release_irq(RTC_IRQ);
	rtai_hw_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
	CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	rtai_hw_sti();
	enabled = 0;
	rt_task_delete(&timer_task);
	rt_free_srq(srq);
	return;
}
