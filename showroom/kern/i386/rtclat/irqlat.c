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

#define USETHIS 1

#define USETIMEOFDAY 0

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
static int tsc_period, maxj, maxj_echo, pasd;

#if USETIMEOFDAY
static volatile int tvi;
static struct timeval tv[2];
#endif

static void rtc_handler (int irq, unsigned long rtc_freq)
{
#if USETIMEOFDAY
	static int stp, cnt;
	if (++cnt == rtc_freq) {
		rt_printk("<> IRQ %d, %d: CNT %d: TIMEOFDAY %lu, %lu <>\n", irq, ++stp, cnt, tv[tvi].tv_sec, tv[tvi].tv_usec);
		cnt = 0;
	}
#endif

	if (pasd) {
		RTIME t;
		int jit;

		t = rdtsc();
		if ((jit = abs((int)(t - t0) - tsc_period)) > maxj) {
			maxj = jit;
		}
		t0 = t;
	} else {
		pasd = 1;
		t0 = rdtsc();
	}
#if USETHIS
 	CMOS_READ(RTC_INTR_FLAGS);
	rt_enable_irq(RTC_IRQ);
#endif
}

#if USETHIS

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

	rt_disable_irq(RTC_IRQ);
	rt_request_irq(RTC_IRQ, (void *)rtc_handler, (void *)rtc_freq, 1);
	rtai_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
	CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	CMOS_WRITE(RTC_REF_CLCK_32KHZ | (16 - pwr2),          RTC_FREQ_SELECT);
	CMOS_WRITE((CMOS_READ(RTC_CONTROL) & 0x8F) | RTC_PIE, RTC_CONTROL);
	CMOS_READ(RTC_INTR_FLAGS);
	rt_enable_irq(RTC_IRQ);
	rtai_sti();
}

static void rtc_stop(void)
{
	rt_disable_irq(RTC_IRQ);
	rt_release_irq(RTC_IRQ);
	rtai_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
	CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	rtai_sti();
}

#endif

static struct timer_list timer;

static void timer_fun(unsigned long none)
{
	int t;
	if (maxj_echo < maxj) {
		maxj_echo = maxj;
		t = imuldiv(maxj_echo, 1000000000, rtai_tunables.cpu_freq);
		printk("INCREASED TO: %d.%-3d (us)\n", t/1000, t%1000); // silly and wrong but acceptable
	}
#if USETIMEOFDAY
	do_gettimeofday(&tv[1 - tvi]);
	tvi = 1 - tvi;
#endif
	mod_timer(&timer, jiffies + ECHO_PERIOD*HZ/1000);
}

int _init_module(void)
{
	init_timer(&timer);
	timer.function = timer_fun;
	mod_timer(&timer, jiffies + ECHO_PERIOD*HZ/1000);
	printk("\nCHECKING WITH PERIOD: %d (us)\n\n", PERIOD/1000);
	tsc_period = imuldiv(PERIOD, rtai_tunables.cpu_freq, 1000000000);
#if USETHIS
	rtc_start(RTC_FREQ);
#else
	rt_request_rtc(RTC_FREQ, (void *)rtc_handler);
#endif
	return 0;
}

void _cleanup_module(void)
{
	int t;

#if USETHIS
	rtc_stop();
#else
	rt_release_rtc();
#endif
	del_timer(&timer);
	t = imuldiv(maxj_echo, 1000000000, rtai_tunables.cpu_freq);
	printk("\nCHECKED WITH PERIOD: %d (us), MAXJ: %d.%-3d (us)\n\n", PERIOD/1000, t/1000, t%1000);
	return;
}

module_init(_init_module);
module_exit(_cleanup_module);
