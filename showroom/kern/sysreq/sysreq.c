/*
COPYRIGHT (C) 1999-2007 Paolo Mantegazza <mantegazza@aero.polimi.it>

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

#include <linux/kernel.h>
#include <linux/module.h>

#include <asm/semaphore.h>
#include <asm/uaccess.h>

#include <asm/rtai.h>

MODULE_LICENSE("GPL");

#define USE_APIC 0
#define TICK 20000000 //ns (!!! CAREFULL NEVER BELOW HZ IF USE_APIC == 0 !!!)

#if !defined(FREQ_APIC) || !defined(RTAI_FREQ_APIC)
#define FREQ_APIC FREQ_8254
#endif

static int srq, scount;

#ifndef DECLARE_MUTEX_LOCKED
#define DECLARE_MUTEX_LOCKED(name) __DECLARE_SEMAPHORE_GENERIC(name,0)
#endif
static DECLARE_MUTEX_LOCKED(sem);

static long long user_srq_handler(unsigned long whatever)
{
	long long time;

	if (whatever == 1) {
		return (long long)(TICK/1000);
	}

	if (whatever == 2) {
#ifdef CONFIG_SMP
		rtai_cli();
		_send_sched_ipi(rtai_cpuid() ? 1 : 2);
		rtai_sti();
#endif
		return (long long)scount;
	}

	down_interruptible(&sem);

// let's show how to communicate. Copy to and from user shall allow any kind of
// data interchange and service.
	time = llimd(rtai_rdtsc(), 1000000, CPU_FREQ);
	copy_to_user((long long *)whatever, &time, sizeof(long long));
	return time;
}

static void rtai_srq_handler(void)
{
	up(&sem);
}

static void sched_ipi_handler(void)
{
#if 0 // diagnose to see if interrupts are coming in
	static int cnt[NR_RT_CPUS];
	int cpuid = rtai_cpuid();
	rt_printk("IPIed CPU: CPU %d, %d\n", cpuid, ++cnt[cpuid]);
#endif
	++scount;
}

static void rt_timer_handler(void)
{

#if 0 // diagnose to see if interrupts are coming in
	static int cnt[NR_RT_CPUS];
	int cpuid = rtai_cpuid();
	rt_printk("TIMER TICK: CPU %d, %d\n", cpuid, ++cnt[cpuid]);
#endif

#if defined(CONFIG_SMP) && USE_APIC
	if (!rtai_cpuid())
#endif
	rt_pend_linux_srq(srq);

#if !USE_APIC
	rt_times.tick_time = rt_times.intr_time;
	rt_times.intr_time = rt_times.tick_time + rt_times.periodic_tick;
	rt_set_timer_delay(0);
	if (rt_times.tick_time >= rt_times.linux_time) {
		if (rt_times.linux_tick > 0) {
			rt_times.linux_time += rt_times.linux_tick;
		}
		rt_pend_linux_irq(TIMER_8254_IRQ);
	}
#endif

	return;
}

int init_module(void)
{
	srq = rt_request_srq(0xcacca, rtai_srq_handler, user_srq_handler);

#if defined(CONFIG_SMP) && USE_APIC
	do {
		int cpuid;
		struct apic_timer_setup_data setup_data[NR_RT_CPUS];
		for (cpuid = 0; cpuid < NR_RT_CPUS; cpuid++) {
			setup_data[cpuid].mode  = 1;
			setup_data[cpuid].count = TICK;
		}
		rt_request_apic_timers(rt_timer_handler, setup_data);
	} while (0);
#else
	rt_request_timer(rt_timer_handler, imuldiv(TICK, USE_APIC ? FREQ_APIC : FREQ_8254, 1000000000), USE_APIC);
#endif

#ifdef CONFIG_SMP
	rt_request_irq(SCHED_IPI, (void *)sched_ipi_handler, NULL, 0);
#endif

	return 0;
}

void cleanup_module(void)
{
#if defined(CONFIG_SMP) && USE_APIC
	rt_free_apic_timers();
#else
	rt_free_timer();
#endif

#ifdef CONFIG_SMP
	rt_release_irq(SCHED_IPI);
#endif

	rt_free_srq(srq);
}
