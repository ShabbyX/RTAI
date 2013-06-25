/*
COPYRIGHT (C) 2000  Andrew Hooper (andrew@best.net.nz)
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
#include <linux/module.h>
*/


#include <linux/module.h>
#include <asm/io.h>

#include <asm/rtai.h>
#include <rtai_sched.h>
#include <rtai_lxrt.h>
#include <rtai_mbx.h>
#include <rtai_registry.h>

static MBX mbx;

static SEM sem;

#define RTC_IRQ  8
#define MAX_RTC_FREQ  8192
#define RTC_FREQ      MAX_RTC_FREQ
#define PERIOD  (1000000000 + RTC_FREQ/2)/RTC_FREQ // nanos
#define IRQ_CPU   (1 << 0)

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

#define pause_io()  \
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

static void rtc_handler(int irq, unsigned long rtc_freq)
{
	static char start = 0;
 	CMOS_READ(RTC_INTR_FLAGS);
	rt_mbx_receive_if(&mbx, &start, 1);
	if (start) {
		rt_sem_signal(&sem);
	}
}

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
	rt_release_irq(RTC_IRQ);
	rtai_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
	CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	CMOS_WRITE(RTC_REF_CLCK_32KHZ | (16 - pwr2),          RTC_FREQ_SELECT);
	CMOS_WRITE((CMOS_READ(RTC_CONTROL) & 0x8F) | RTC_PIE, RTC_CONTROL);
	rt_request_irq(RTC_IRQ, (void *)rtc_handler, (void *)rtc_freq, 0);
	rt_enable_irq(RTC_IRQ);
	CMOS_READ(RTC_INTR_FLAGS);
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

int init_module(void)
{
	rt_mbx_init(&mbx, 1);
	rt_register(nam2num("RESMBX"), &mbx, IS_MBX, current);
	rt_sem_init(&sem, 0);
	rt_register(nam2num("RESEM"), &sem, IS_SEM, current);
	rt_assign_irq_to_cpu(RTC_IRQ, IRQ_CPU);
	rtc_start(RTC_FREQ);
	return 0;
}

void cleanup_module(void)
{
	rt_reset_irq_to_sym_mode(RTC_IRQ);
	rtc_stop();
	rt_mbx_delete(&mbx);
	rt_sem_delete(&sem);
}
