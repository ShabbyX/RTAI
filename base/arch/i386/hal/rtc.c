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

#if CONFIG_RTAI_DONT_DISPATCH_CORE_IRQS

int _rtai_rtc_timer_handler(void)
{
	unsigned long cpuid = rtai_cpuid();
	unsigned long sflags;

	HAL_LOCK_LINUX();
	rt_mask_and_ack_irq(RTC_IRQ);
	RTAI_SCHED_ISR_LOCK();

	RT_CMOS_READ(RTC_INTR_FLAGS); // CMOS_READ(RTC_INTR_FLAGS);
	rt_enable_irq(RTC_IRQ);
	usr_rtc_handler();

	RTAI_SCHED_ISR_UNLOCK();
	HAL_UNLOCK_LINUX();

	if (!test_bit(IPIPE_STALL_FLAG, ROOT_STATUS_ADR(cpuid))) {
		rtai_sti();
		hal_fast_flush_pipeline(cpuid);
#if defined(CONFIG_SMP) &&  LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,32)
		__set_bit(IPIPE_STALL_FLAG, ROOT_STATUS_ADR(cpuid));
#endif
		return 1;
	}

	return 0;
}

void rtai_rtc_timer_handler(void);
DEFINE_VECTORED_ISR(rtai_rtc_timer_handler, _rtai_rtc_timer_handler);

static struct desc_struct rtai_rtc_timer_sysvec;

#endif /* CONFIG_RTAI_DONT_DISPATCH_CORE_IRQS */

/*
 * NOTE FOR USE IN RTAI_TRIOSS.
 * "rtc_freq" must be a power of 2 & (MIN_RTC_FREQ <= rtc_freq <= MAX_RTC_FREQ).
 * So the best thing to do is to load this module and your skin of choice
 * setting "rtc_freq" in this module and "rtc_freq" in the skin specific module
 * to the very same power of 2 that best fits your needs.
 */

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

int fusion_timer_running;
#ifdef RTAI_TRIOSS
static void fusion_rtc_handler(void)
{
#ifdef CONFIG_X86_LOCAL_APIC
#include <asm/apic.h>
#include <mach_ipi.h>
	int cpuid;
	for (cpuid = 0; cpuid < num_online_cpus(); cpuid++) {
		hal_pend_domain_uncond(RTAI_APIC_TIMER_VECTOR, fusion_domain, cpuid);
	}
#ifdef CONFIG_SMP
	send_IPI_allbutself(RESCHEDULE_VECTOR);  // any unharmful ipi suffices
#endif
#else
	hal_pend_domain_uncond(RTAI_TIMER_8254_IRQ, fusion_domain, rtai_cpuid());
#endif
}
EXPORT_SYMBOL(fusion_timer_running);
#endif

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
#ifdef RTAI_TRIOSS
	usr_rtc_handler = fusion_rtc_handler;
#else
	usr_rtc_handler = handler ? handler : rt_broadcast_rtc_interrupt;
#endif
	SET_FUSION_TIMER_RUNNING();
#ifdef TEST_RTC
	rt_printk("<%s>\n", fusion_timer_running ? "FUSION TIMER RUNNING" : "");
#endif
	rt_request_irq(RTC_IRQ, (void *)rtc_handler, (void *)rtc_freq, 0);
	SET_INTR_GATE(ext_irq_vector(RTC_IRQ), rtai_rtc_timer_handler, rtai_rtc_timer_sysvec);
	rt_enable_irq(RTC_IRQ);
	CMOS_READ(RTC_INTR_FLAGS);
	return;
}

void rt_release_rtc(void)
{
	rt_disable_irq(RTC_IRQ);
	usr_rtc_handler = NULL;
	CLEAR_FUSION_TIMER_RUNNING();
	RESET_INTR_GATE(ext_irq_vector(RTC_IRQ), rtai_rtc_timer_sysvec);
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
