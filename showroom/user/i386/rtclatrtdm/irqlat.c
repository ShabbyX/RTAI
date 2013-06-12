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

#include <rtai.h>
#include <rtdm/rtdm_driver.h>

#define MAX_RTC_FREQ  8192
#define RTC_FREQ      MAX_RTC_FREQ

int PERIOD = (1000000000 + RTC_FREQ/2)/RTC_FREQ; // nanos
RTAI_MODULE_PARM(PERIOD, int);

int ECHO_PERIOD = 1000; // ms
RTAI_MODULE_PARM(ECHO_PERIOD, int);

#define RTC_IRQ  8

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

static RTIME t0;
static int tsc_period, maxj, maxj_echo, pasd = RTC_FREQ;

static int rtc_handler(rtdm_irq_t *none)
{
	if (!pasd) {
		RTIME t;
		int jit;

		t = rdtsc();
		if ((jit = abs((int)(t - t0) - tsc_period)) > maxj) {
			maxj = jit;
		}
		t0 = t;
	} else {
		pasd--;
		t0 = rdtsc();
	}
 	CMOS_READ(RTC_INTR_FLAGS);
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

static struct timer_list timer;

static void timer_fun(unsigned long none)
{
	int t;
	if (maxj_echo < maxj) {
		maxj_echo = maxj;
		t = imuldiv(maxj_echo, 1000000000, rtai_tunables.cpu_freq);
		printk("INCREASED TO: %d.%-3d (us)\n", t/1000, t%1000); // silly and wrong but acceptable
	}
	mod_timer(&timer, jiffies + ECHO_PERIOD*HZ/1000);
}

int _init_module(void)
{
	init_timer(&timer);
	timer.function = timer_fun;
	mod_timer(&timer, jiffies + ECHO_PERIOD*HZ/1000);
	printk("\nCHECKING WITH PERIOD: %d (us)\n\n", PERIOD/1000);
	tsc_period = imuldiv(PERIOD, rtai_tunables.cpu_freq, 1000000000);
	rtc_start(RTC_FREQ);
	return 0;
}

void _cleanup_module(void)
{
	int t;

	rtc_stop();
	del_timer(&timer);
	t = imuldiv(maxj_echo, 1000000000, rtai_tunables.cpu_freq);
	printk("\nCHECKED WITH PERIOD: %d (us), MAXJ: %d.%-3d (us)\n\n", PERIOD/1000, t/1000, t%1000);
	return;
}

module_init(_init_module);
module_exit(_cleanup_module);
