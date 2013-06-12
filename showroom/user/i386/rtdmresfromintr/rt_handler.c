/*
 * Copyright (C) 2008 Paolo Mantegazza <mantegazza@aero.polimi.it>.
 *
 * RTAI/fusion is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * RTAI/fusion is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RTAI/fusion; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/module.h>
#include <asm/io.h>

#include "params.h"
#include <rtdm/rtdm_driver.h>

int ECHO_PERIOD = 1000; // ms
RTAI_MODULE_PARM(ECHO_PERIOD, int);

#define RTC_REG_A  10
#define RTC_REG_B  11
#define RTC_REG_C  12

#define RTC_CONTROL      RTC_REG_B
#define RTC_INTR_FLAGS   RTC_REG_C
#define RTC_FREQ_SELECT  RTC_REG_A

#define RTC_REF_CLCK_32KHZ  0x20
#define RTC_PIE             0x40

#define RTC_PORT(x)     (0x70 + (x))
#define RTC_ALWAYS_BCD  1

#define pause_io() \
	do { asm volatile("outb %%al,$0x80" : : : "memory"); } while (0)

static inline unsigned char CMOS_READ(unsigned char addr)
{
	outb((addr),RTC_PORT(0));
	pause_io();
	return inb(RTC_PORT(1));
}

#define CMOS_WRITE(val, addr) ({ \
	outb((addr),RTC_PORT(0)); \
	pause_io(); \
	outb((val),RTC_PORT(1)); \
	pause_io(); \
})

static int rtc_handler(rtdm_irq_t *none)
{
	RT_TASK *task;
 	CMOS_READ(RTC_INTR_FLAGS);
	if ((task = rt_get_adr(nam2num("PRCTSK")))) {
		rt_task_resume(task);
	}
	return 0;
}

rtdm_irq_t irqh;

#define MIN_RTC_FREQ  2

static void rtc_start(long rtc_freq)
{
	int pwr2;

	if (rtc_freq <= 0) {
		rtc_freq = RTC_FREQ;
	}
	if (rtc_freq > MAX_RTC_FREQ) {
		rtc_freq = MAX_RTC_FREQ;
	} else if (rtc_freq < MIN_RTC_FREQ) {
		rtc_freq = MIN_RTC_FREQ;
	}
	pwr2 = 1;
	if (rtc_freq > MIN_RTC_FREQ) {
		while (rtc_freq > (1 << pwr2)) {
			pwr2++;
		}
		if (rtc_freq <= ((3*(1 << (pwr2 - 1)) + 1)>>1)) {
			pwr2--;
		}
	}

	rtdm_irq_request(&irqh, RTC_IRQ, rtc_handler, 0, "RTC", NULL);
	rtai_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
	CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	CMOS_WRITE(RTC_REF_CLCK_32KHZ | (16 - pwr2),          RTC_FREQ_SELECT);
	CMOS_WRITE((CMOS_READ(RTC_CONTROL) & 0x8F) | RTC_PIE, RTC_CONTROL);
	CMOS_READ(RTC_INTR_FLAGS);
	rtai_sti();
}

static void rtc_stop(void)
{
	rtdm_irq_free(&irqh);
	rtai_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
	CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	rtai_sti();
}

int _init_module(void)
{
	rt_assign_irq_to_cpu(RTC_IRQ, IRQ_CPU);
	rtc_start(RTC_FREQ);
	return 0;
}

void _cleanup_module(void)
{
	rt_reset_irq_to_sym_mode(RTC_IRQ);
	rtc_stop();
}

module_init(_init_module);
module_exit(_cleanup_module);
