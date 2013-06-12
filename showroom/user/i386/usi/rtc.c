/*
COPYRIGHT (C) 2008  Paolo Mantegazza (mantegazza@aero.polimi.it)

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


/*
   For an easy use in user space made independent from
   Linux includes by copying from it all of what needed
*/

#include <asm/io.h>

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
	outb(addr,RTC_PORT(0));
	pause_io();
	return inb(RTC_PORT(1));
}

#define CMOS_WRITE(val, addr) ({ \
	outb((addr),RTC_PORT(0)); \
	pause_io(); \
	outb((val),RTC_PORT(1)); \
	pause_io(); \
})

//#define TEST_RTC
#define MIN_RTC_FREQ  2
#define MAX_RTC_FREQ  8192
#define RTC_FREQ      MAX_RTC_FREQ

static inline void rtc_enable_irq(int irq, int rtc_freq)
{
#ifdef TEST_RTC
	static int stp, cnt;
	if (++cnt == rtc_freq) {
		rt_printk("<> IRQ %d, %d: CNT %d <>\n", irq, ++stp, cnt);
		cnt = 0;
	}
#endif
 	CMOS_READ(RTC_INTR_FLAGS);
	rt_enable_irq(RTC_IRQ);
}

void rtc_start(long rtc_freq)
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
	rtai_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
	CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	CMOS_WRITE(RTC_REF_CLCK_32KHZ | (16 - pwr2),          RTC_FREQ_SELECT);
	CMOS_WRITE((CMOS_READ(RTC_CONTROL) & 0x8F) | RTC_PIE, RTC_CONTROL);
	rtai_sti();
	rt_enable_irq(RTC_IRQ);
	CMOS_READ(RTC_INTR_FLAGS);
	return;
}

void rtc_stop(void)
{
	rt_disable_irq(RTC_IRQ);
	rtai_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
	CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	rtai_sti();
	return;
}
