/*
COPYRIGHT (C) 2013 Paolo Mantegazza (mantegazza@aero.polimi.it)

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

#include <linux/semaphore.h>
#include <asm/uaccess.h>

#include <asm/rtai.h>
#include <rtai_schedcore.h>
#include <rtai_wrappers.h>

MODULE_LICENSE("GPL");

#define DIAGSRQ   0  // diagnose srq arg values
#define DIAGSLTMR 0  // diagnose if LINUX time is running
#define DIAGIPI   0  // diagnose if IPIs are received
#define DIAGHRTMR 0  // diagnose if RTAI hard time is running

#define LINUX_HZ_PERCENT 100 // !!! 1 to 100 !!!

#define LINUX_TIMER_FREQ ((HZ*LINUX_HZ_PERCENT)/100)

static int srq, ipi_count, tmr_count;

static DECLARE_MUTEX_LOCKED(sem);

static long long user_srq_handler(unsigned long req)
{
	int semret, cpyret;
	long long time;

#if DIAGSRQ
	printk("SRQ REQUEST %lu\n", req);
#endif
	switch (req) {
		case 1: {
			return (long long)(HZ/LINUX_TIMER_FREQ);
		}
		case 2: {
			return (long long)(HZ);
		}
		case 3: {
			return (long long)tmr_count;
		}
		case 4: {
#if DIAGIPI
			printk("SEND IPI FROM CPU: %d, TO CPU: %d, AT TSCTIME: %lld\n", rtai_cpuid(), rtai_cpuid() ? 0 : 1, rtai_rdtsc());
#endif
                	rtai_cli();
	                send_sched_ipi(rtai_cpuid() ? 1 : 2);
        	        rtai_sti();
			return (long long)ipi_count;
		}
	}

	semret = down_interruptible(&sem);

// let's show how to communicate. Copy to and from user shall allow any kind of
// data interchange and service.
	time = llimd(rtai_rdtsc(), 1000000, CPU_FREQ);
	cpyret = copy_to_user((long long *)req, &time, sizeof(long long));
	return time;
}

static void rtai_srq_handler(void)
{
	up(&sem);
}

static struct timer_list timer;

static void rt_soft_linux_timer_handler(unsigned long none)
{
#if DIAGSLTMR
	static int cnt[NR_RT_CPUS];
	int cpuid = rtai_cpuid();
	rt_printk("LINUX TIMER TICK: CPU %d, %d\n", cpuid, ++cnt[cpuid]);
#endif
	rt_pend_linux_srq(srq);
	mod_timer(&timer, jiffies + (HZ/LINUX_TIMER_FREQ));
	return;
}

extern void *rt_linux_hrt_next_shot;

int _rt_linux_hrt_next_shot(unsigned long deltat, void *hrt_dev)
{
	rtai_cli();
	rt_set_timer_delay(imuldiv(deltat, TIMER_FREQ, 1000000000));
	rtai_sti();
	return 0;
}

static int ltcnt;
static void rt_rtai_timer_handler(int irq)
{
        int cpuid = rtai_cpuid();
#if DIAGHRTMR
        static int cnt[NR_RT_CPUS];
	printk("RECVD RTAI TIMER(s) IRQ %d AT CPU: %d, CNT: %d, AT TSCTIME: %lld\n", irq, cpuid, ++cnt[cpuid], rtai_rdtsc());
#endif
	if (irq == LOCAL_TIMER_IPI) {
		printk("<<< CPU: %d, RECEIVED LOCAL_TIMER_IPI IRQ: %d, COUNT: %d >>>\n", cpuid, irq, ++ltcnt);
		update_linux_timer(cpuid);
		return;
	}
	update_linux_timer(cpuid);
	++tmr_count;
}

#ifdef CONFIG_SMP
static void sched_ipi_handler(void)
{
#if DIAGIPI
        static int cnt[NR_RT_CPUS];
        int cpuid = rtai_cpuid();
	printk("RECVD IPI AT CPU: %d, CNT: %d, AT TSCTIME: %lld\n", cpuid, ++cnt[cpuid], rtai_rdtsc());
#endif
	++ipi_count;
}
#endif

int init_module(void)
{
	printk("RTAI_APIC_TIMER_VECTOR %d, RTAI_APIC_TIMER_IPI %d, LOCAL_TIMER_VECTOR %d, LOCAL_TIMER_IPI %d, TIMER FREQ %u.\n", RTAI_APIC_TIMER_VECTOR, RTAI_APIC_TIMER_IPI, LOCAL_TIMER_VECTOR, LOCAL_TIMER_IPI, (unsigned int)TIMER_FREQ);
	srq = rt_request_srq(0xbeffa, rtai_srq_handler, user_srq_handler);
        init_timer(&timer);
        timer.function = rt_soft_linux_timer_handler;
	mod_timer(&timer, jiffies + (HZ/LINUX_TIMER_FREQ));
        rt_linux_hrt_next_shot = _rt_linux_hrt_next_shot;
#ifdef CONFIG_SMP
do {
	void *setup_data;
	rt_request_irq(SCHED_IPI, (void *)sched_ipi_handler, NULL, 0);
	setup_data = kzalloc(sizeof(struct apic_timer_setup_data)*num_online_cpus(), GFP_KERNEL);
	rt_request_apic_timers((void *)rt_rtai_timer_handler, setup_data);
	kfree(setup_data);
} while (0);
#else
	rt_request_timer((void *)rt_rtai_timer_handler, 0, TIMER_TYPE);
#endif
        return 0;
}

void cleanup_module(void)
{
        del_timer(&timer);
	rt_free_srq(srq);
        rt_linux_hrt_next_shot = NULL;
#ifdef CONFIG_SMP
	rt_release_irq(SCHED_IPI);
	rt_free_apic_timers();
#else
	rt_free_timer();
#endif
	printk("*** TOTAL RECEIVED LOCAL_TIMER_IPI IRQ %d ***\n", ltcnt);
}
