/*
 * Copyright (C) 2005-2008  Paolo Mantegazza  (mantegazza@aero.polimi.it)
 * (RTC specific part with) Giuseppe Quaranta (quaranta@aero.polimi.it)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifdef INCLUDED_BY_HAL_C

#include <linux/mc146818rtc.h>
static inline unsigned char RT_CMOS_READ(unsigned char addr)
{
	outb_p(addr, RTC_PORT(0));
	return inb_p(RTC_PORT(1));
}

//#define TEST_RTC
#define MIN_RTC_FREQ  2
#define MAX_RTC_FREQ  8192
#define RTC_FREQ      MAX_RTC_FREQ

static void rt_broadcast_rtc_interrupt(void)
{
#ifdef CONFIG_SMP
	apic_wait_icr_idle();
	apic_write_around(APIC_ICR, APIC_DM_FIXED | APIC_DEST_ALLBUT | RTAI_APIC_TIMER_VECTOR | APIC_DEST_LOGICAL);
	((void (*)(void))rtai_realtime_irq[RTAI_APIC_TIMER_IPI].handler)();
#endif
}

static void (*usr_rtc_handler)(void);

static void rtc_handler(int irq, int rtc_freq)
{
#ifdef TEST_RTC
	static int stp, cnt;
	if (++cnt == rtc_freq) {
		rt_printk("<> IRQ %d, %d: CNT %d <>\n", irq, ++stp, cnt);
		cnt = 0;
	}
#endif
 	RT_CMOS_READ(RTC_INTR_FLAGS); // CMOS_READ(RTC_INTR_FLAGS);
	rt_enable_irq(RTC_IRQ);
	if (usr_rtc_handler) {
		usr_rtc_handler();
	}
}

void rt_request_rtc(long rtc_freq, void *handler)
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
	rt_release_irq(RTC_IRQ);
	rtai_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
	CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	CMOS_WRITE(RTC_REF_CLCK_32KHZ | (16 - pwr2),          RTC_FREQ_SELECT);
	CMOS_WRITE((CMOS_READ(RTC_CONTROL) & 0x8F) | RTC_PIE, RTC_CONTROL);
	rtai_sti();
	usr_rtc_handler = handler ? handler : rt_broadcast_rtc_interrupt;
	rt_request_irq(RTC_IRQ, (void *)rtc_handler, (void *)rtc_freq, 0);
	rt_enable_irq(RTC_IRQ);
	CMOS_READ(RTC_INTR_FLAGS);
	return;
}

void rt_release_rtc(void)
{
	rt_disable_irq(RTC_IRQ);
	usr_rtc_handler = NULL;
	rt_release_irq(RTC_IRQ);
	rtai_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
	CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	rtai_sti();
	return;
}

EXPORT_SYMBOL(rt_request_rtc);
EXPORT_SYMBOL(rt_release_rtc);

#endif /* INCLUDED_BY_HAL_C */
