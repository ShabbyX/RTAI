/**
 *   @ingroup hal
 *   @file
 *
 *   ARTI -- RTAI-compatible Adeos-based Real-Time Interface. Based on
 *   the original RTAI layer for x86.
 *
 *   Original RTAI/x86 layer implementation: \n
 *   Copyright &copy; 2000 Paolo Mantegazza <mantegazza@aero.polimi.it> \n
 *   Copyright &copy; 2000 Steve Papacharalambous <stevep@freescale.com> \n
 *   Copyright &copy; 2000 Stuart Hughes <stuarth@lineo.com> \n
 *   and others.
 *
 *   RTAI/x86 rewrite over Adeos: \n
 *   Copyright &copy 2002 Philippe Gerum <rpm@xenomai.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * @addtogroup hal
 *@{*/


#ifndef _RTAI_ASM_M68KNOMMU_HAL_H
#define _RTAI_ASM_M68KNOMMU_HAL_H

#include <linux/version.h>

/*#if defined(CONFIG_REGPARM)
#define RTAI_SYSCALL_MODE __attribute__((regparm(0)))
#else*/
#define RTAI_SYSCALL_MODE
/*#endif*/

#define RTAI_DUOSS
#define LOCKED_LINUX_IN_IRQ_HANDLER
#define DOMAIN_TO_STALL  (fusion_domain)

#include <rtai_hal_names.h>
#include <asm/rtai_vectors.h>
#include <rtai_types.h>

#define RTAI_NR_CPUS  1

#define FIRST_EXTERNAL_VECTOR 0x40

#ifndef _RTAI_FUSION_H
static __inline__ unsigned long ffnz (unsigned long word) {
    /* Derived from bitops.h's ffs() */
    int r = 1;
    if (!(word & 0xff)) {
	word >>= 8;
	r += 8;
    }
    if (!(word & 0xf)) {
	word >>= 4;
	r += 4;
    }
    if (!(word & 3)) {
	word >>= 2;
	r += 2;
    }
    if (!(word & 1)) {
	word >>= 1;
	r += 1;
    }
    return r - 1;
}
#endif

static inline unsigned long long rtai_ulldiv (unsigned long long ull,
					      unsigned long uld,
					      unsigned long *r) {

    unsigned long long qf, rf, q, p;
    unsigned long tq, rh;
    p = ull;
    q = 0;
    rf = 0x100000000ULL - (qf = 0xFFFFFFFFUL / uld) * uld;
    while (p >= uld) {
	q += (((unsigned long long)(tq = ((unsigned long)(p >> 32)) / uld)) << 32);
	rh = ((unsigned long)(p >> 32)) - tq * uld;
	q += rh * qf + (tq = (unsigned long)p / uld);
	p  = rh * rf + ((unsigned long)p - tq * uld);
    }
    if (r)
	*r = p;
    return q;
}

static inline int rtai_imuldiv (int i, int mult, int div) {

    /* Returns (int)i = (int)i*(int)(mult)/(int)div. */
    long long temp = i * (long long)mult;
    return rtai_ulldiv(temp, div, NULL);
}

static inline unsigned long long rtai_llimd(unsigned long long ll, unsigned int mult, unsigned int div) {
	unsigned long long res, tmp;
	unsigned long r;
	res = rtai_ulldiv(ll,div,&r) * mult;
	tmp = r * (unsigned long long)mult;
	res += rtai_ulldiv(tmp, div, NULL);
	return res;
}



#if defined(__KERNEL__) && !defined(__cplusplus)
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/rtai_atomic.h>
#include <asm/rtai_fpu.h>
#include <rtai_trace.h>

extern asmlinkage int fprintk(const char *fmt, ...);

struct rtai_realtime_irq_s {
	int (*handler)(unsigned irq, void *cookie);
	void *cookie;
	int retmode;
	int cpumask;
	int (*irq_ack)(unsigned int);
};

/*
 * Linux has this information in io_apic.c, but it does not export it;
 * on the other hand it should be fairly stable this way and so we try
 * to avoid putting something else in our patch.
 */

static inline int ext_irq_vector(int irq)
{
	if (irq != 2) {
		return (FIRST_EXTERNAL_VECTOR + irq);
	}
	return -EINVAL;
}

#define RTAI_DOMAIN_ID  0x9ac15d93  // nam2num("rtai_d")
#define RTAI_NR_TRAPS   HAL_NR_FAULTS
#define RTAI_NR_SRQS    32

#define RTAI_APIC_TIMER_VECTOR    RTAI_APIC_HIGH_VECTOR
#define RTAI_APIC_TIMER_IPI       RTAI_APIC_HIGH_IPI
#define RTAI_SMP_NOTIFY_VECTOR    RTAI_APIC_LOW_VECTOR
#define RTAI_SMP_NOTIFY_IPI       RTAI_APIC_LOW_IPI

#define RTAI_TIMER_LINUX_IRQ		96
#define RT_TIMER_IRQ				96
#define RTAI_FREQ_8254            MCF_BUSCLK
#define RTAI_COUNTER_2_LATCH      0xfffe
#define RTAI_LATENCY_8254         CONFIG_RTAI_SCHED_8254_LATENCY
#define RTAI_SETUP_TIME_8254      8011

#define RTAI_CALIBRATED_APIC_FREQ 0
#define RTAI_FREQ_APIC            (rtai_tunables.apic_freq)
#define RTAI_LATENCY_APIC         CONFIG_RTAI_SCHED_APIC_LATENCY
#define RTAI_SETUP_TIME_APIC      1000

#define RTAI_TIME_LIMIT            0x7000000000000000LL

#define rtai_cpuid()      hal_processor_id()
#define rtai_tskext(idx)  hal_tskext[idx]

/* Use these to grant atomic protection when accessing the hardware */
#define rtai_hw_cli()                  hal_hw_cli()
#define rtai_hw_sti()                  hal_hw_sti()
#define rtai_hw_save_flags_and_cli(x)  hal_hw_local_irq_save(x)
#define rtai_hw_restore_flags(x)       hal_hw_local_irq_restore(x)
#define rtai_hw_save_flags(x)          hal_hw_local_irq_flags(x)

/* Use these to grant atomic protection in hard real time code */
#define rtai_cli()                  hal_hw_cli()
#define rtai_sti()                  hal_hw_sti()
#define rtai_save_flags_and_cli(x)  hal_hw_local_irq_save(x)
#define rtai_restore_flags(x)       hal_hw_local_irq_restore(x)
#define rtai_save_flags(x)          hal_hw_local_irq_flags(x)

static inline struct hal_domain_struct *get_domain_pointer(int n)
{
	struct list_head *p = hal_pipeline.next;
	struct hal_domain_struct *d;
	unsigned long i = 0;
	while (p != &hal_pipeline) {
		d = list_entry(p, struct hal_domain_struct, p_link);
		if (++i == n) {
			return d;
		}
		p = d->p_link.next;
	}
	return (struct hal_domain_struct *)i;
}

#define ROOT_STATUS_ADR(cpuid)  (&ipipe_cpudom_var(hal_root_domain, status))
#define ROOT_STATUS_VAL(cpuid)  (ipipe_cpudom_var(hal_root_domain, status))

#define hal_pend_domain_uncond(irq, domain, cpuid) \
do { \
	if (likely(!test_bit(IPIPE_LOCK_FLAG, &(domain)->irqs[irq].control))) { \
		__set_bit((irq) & IPIPE_IRQ_IMASK, &ipipe_cpudom_var(domain, irqpend_lomask)[(irq) >> IPIPE_IRQ_ISHIFT]); \
		__set_bit((irq) >> IPIPE_IRQ_ISHIFT, &ipipe_cpudom_var(domain, irqpend_himask)); \
	} else { \
		__set_bit((irq) & IPIPE_IRQ_IMASK, &ipipe_cpudom_var(domain, irqheld_mask)[(irq) >> IPIPE_IRQ_ISHIFT]); \
	} \
	ipipe_cpudom_var(domain, irqall)[irq]++; \
} while (0)

#define hal_fast_flush_pipeline(cpuid) \
do { \
	if (ipipe_cpudom_var(hal_root_domain, irqpend_himask) != 0) { \
		rtai_cli(); \
		hal_sync_stage(IPIPE_IRQMASK_ANY); \
	} \
} while (0)

#define hal_pend_uncond(irq, cpuid)  hal_pend_domain_uncond(irq, hal_root_domain, cpuid)

extern volatile unsigned long *ipipe_root_status[];

#define hal_test_and_fast_flush_pipeline(cpuid) \
do { \
	if (!test_bit(IPIPE_STALL_FLAG, ROOT_STATUS_ADR(cpuid))) { \
		hal_fast_flush_pipeline(cpuid); \
		rtai_sti(); \
	} \
} while (0)

#ifdef CONFIG_PREEMPT
#define rtai_save_and_lock_preempt_count() \
	do { int *prcntp, prcnt; prcnt = xchg(prcntp = &preempt_count(), 1);
#define rtai_restore_preempt_count() \
	     *prcntp = prcnt; } while (0)
#else
#define rtai_save_and_lock_preempt_count();
#define rtai_restore_preempt_count();
#endif

typedef int (*rt_irq_handler_t)(unsigned irq, void *cookie);

#define RTAI_CALIBRATED_CPU_FREQ   0
#define RTAI_CPU_FREQ              (rtai_tunables.cpu_freq)

extern long long rdtsc(void);

#define rtai_rdtsc() rdtsc()

struct calibration_data {

    unsigned long cpu_freq;
    unsigned long apic_freq;
    int latency;
    int setup_time_TIMER_CPUNIT;
    int setup_time_TIMER_UNIT;
    int timers_tol[RTAI_NR_CPUS];
};

struct apic_timer_setup_data {

    int mode;
    int count;
};

extern struct rt_times rt_times;

extern struct rt_times rt_smp_times[RTAI_NR_CPUS];

extern struct calibration_data rtai_tunables;

extern volatile unsigned long rtai_cpu_lock[];

//#define RTAI_TASKPRI 0xf0  // simplest usage without changing Linux code base
#define SET_TASKPRI(cpuid)
#define CLR_TASKPRI(cpuid)

extern struct rtai_switch_data {
	volatile unsigned long sflags;
	volatile unsigned long lflags;
} rtai_linux_context[RTAI_NR_CPUS];

irqreturn_t rtai_broadcast_to_local_timers(int irq,
					   void *dev_id,
					   struct pt_regs *regs);

static inline unsigned long rtai_save_flags_irqbit(void)
{
	unsigned long flags;
	rtai_save_flags(flags);
	return !(flags & ~ALLOWINT);
}

static inline unsigned long rtai_save_flags_irqbit_and_cli(void)
{
	unsigned long flags;
	rtai_save_flags_and_cli(flags);
	return !(flags & ~ALLOWINT);
}

#define _send_sched_ipi(dest)

#define rt_spin_lock(lock)
#define rt_spin_unlock(lock)

#define rt_spin_lock_irq(lock)    do { rtai_cli(); } while (0)
#define rt_spin_unlock_irq(lock)  do { rtai_sti(); } while (0)

static inline unsigned long rt_spin_lock_irqsave(spinlock_t *lock)
{
	unsigned long flags;
	rtai_save_flags_and_cli(flags);
	return flags;
}
#define rt_spin_unlock_irqrestore(flags, lock)  do { rtai_restore_flags(flags); } while (0)

#define rt_get_global_lock()      do { rtai_cli(); } while (0)
#define rt_release_global_lock()

#define rt_global_cli()  do { rtai_cli(); } while (0)
#define rt_global_sti()  do { rtai_sti(); } while (0)

static inline unsigned long rt_global_save_flags_and_cli(void)
{
	unsigned long flags;
	rtai_save_flags_and_cli(flags);
	return flags;
}
#define rt_global_restore_flags(flags)  do { rtai_restore_flags(flags); } while (0)

#define rt_global_save_flags(flags)     do { rtai_save_flags(*flags); } while (0)

asmlinkage int rt_printk(const char *format, ...);
asmlinkage int rt_sync_printk(const char *format, ...);

extern struct hal_domain_struct rtai_domain;
extern struct hal_domain_struct *fusion_domain;

#define _rt_switch_to_real_time(cpuid) \
do { \
	rtai_linux_context[cpuid].lflags = xchg(ROOT_STATUS_ADR(cpuid), (1 << IPIPE_STALL_FLAG)); \
	rtai_linux_context[cpuid].sflags = 1; \
	hal_current_domain(cpuid) = &rtai_domain; \
} while (0)

#define rt_switch_to_linux(cpuid) \
do { \
	if (rtai_linux_context[cpuid].sflags) { \
		hal_current_domain(cpuid) = hal_root_domain; \
		ROOT_STATUS_VAL(cpuid) = rtai_linux_context[cpuid].lflags; \
		rtai_linux_context[cpuid].sflags = 0; \
		CLR_TASKPRI(cpuid); \
	} \
} while (0)

#define rt_switch_to_real_time(cpuid) \
do { \
	if (!rtai_linux_context[cpuid].sflags) { \
		_rt_switch_to_real_time(cpuid); \
	} \
} while (0)

#define ack_bad_irq hal_ack_system_irq // linux does not export ack_bad_irq

#define rtai_init_taskpri_irqs() do { } while (0)

static inline int rt_save_switch_to_real_time(int cpuid)
{
	SET_TASKPRI(cpuid);
	if (!rtai_linux_context[cpuid].sflags) {
		_rt_switch_to_real_time(cpuid);
		return 0;
	}
	return 1;
}

#define rt_restore_switch_to_linux(sflags, cpuid) \
do { \
	if (!sflags) { \
		rt_switch_to_linux(cpuid); \
	} else if (!rtai_linux_context[cpuid].sflags) { \
		SET_TASKPRI(cpuid); \
		_rt_switch_to_real_time(cpuid); \
	} \
} while (0)

#define in_hrt_mode(cpuid)  (rtai_linux_context[cpuid].sflags)

#include <asm/coldfire.h>
#include <asm/mcftimer.h>
#include <asm/mcfsim.h>
#include <asm/io.h>

#define TA(a) (MCF_MBAR + MCFTIMER_BASE1 + (a))

#if defined(CONFIG_M532x)
#define	__raw_readtrr	__raw_readl
#define	__raw_writetrr	__raw_writel
#else
#define	__raw_readtrr	__raw_readw
#define	__raw_writetrr	__raw_writew
#endif

static inline void rt_set_timer_delay (int delay) {

	if (delay) {
//		__raw_writetrr(__raw_readtrr(TA(MCFTIMER_TCN)) + delay, TA(MCFTIMER_TRR));

//Please do not laugh, it works better....
//It's just to avoid some fucks with a cache
		asm volatile(" \
		bra 1f\n\t \
		2: \
		movel %1, %%d1\n\t \
		addl %0, %%d1\t\n \
		movel %%d1, %2\n\t \
		bra 3f\n\t \
		1: \
		bra 2b\n\t \
		3: \
		": :"d" (delay), "m"(*(long*)(void*)TA(MCFTIMER_TCN)), "m"(*(long*)(void*)TA(MCFTIMER_TRR)) : "memory", "d1");
		read_timer_cnt();
	}
	else
	{
		__raw_writetrr(rt_smp_times[0].intr_time, TA(MCFTIMER_TRR));
	}
}

    /* Private interface -- Internal use only */

unsigned long rtai_critical_enter(void (*synch)(void));

void rtai_critical_exit(unsigned long flags);

int rtai_calibrate_8254(void);

void rtai_set_linux_task_priority(struct task_struct *task,
				  int policy,
				  int prio);

long rtai_catch_event (struct hal_domain_struct *domain, unsigned long event, int (*handler)(unsigned long, void *));

#endif /* __KERNEL__ && !__cplusplus */

    /* Public interface */

#ifdef __KERNEL__

#include <linux/kernel.h>

#define rtai_print_to_screen  rt_printk

void *ll2a(long long ll, char *s);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int rt_request_irq(unsigned irq,
		   int (*handler)(unsigned irq, void *cookie),
		   void *cookie,
		   int retmode);

int rt_release_irq(unsigned irq);

int rt_set_irq_ack(unsigned int irq, int (*irq_ack)(unsigned int));

static inline int rt_request_irq_wack(unsigned irq, int (*handler)(unsigned irq, void *cookie), void *cookie, int retmode, int (*irq_ack)(unsigned int))
{
	int retval;
	if ((retval = rt_request_irq(irq, handler, cookie, retmode)) < 0) {
		return retval;
	}
	return rt_set_irq_ack(irq, irq_ack);
}

void rt_set_irq_cookie(unsigned irq, void *cookie);

void rt_set_irq_retmode(unsigned irq, int fastret);

/**
 * @name Programmable Interrupt Controllers (PIC) management functions.
 *
 *@{*/
unsigned rt_startup_irq(unsigned irq);

void rt_shutdown_irq(unsigned irq);

void rt_enable_irq(unsigned irq);

void rt_disable_irq(unsigned irq);

void rt_mask_and_ack_irq(unsigned irq);

void rt_unmask_irq(unsigned irq);

void rt_ack_irq(unsigned irq);

/*@}*/

struct desc_struct {
    void *a;
};

struct desc_struct rtai_set_gate_vector (unsigned vector, int type, int dpl, void *handler);

void rtai_reset_gate_vector(unsigned vector, struct desc_struct e);

void rt_do_irq(unsigned irq);

int rt_request_linux_irq(unsigned irq,
			 void *handler,
			 char *name,
			 void *dev_id);

int rt_free_linux_irq(unsigned irq,
		      void *dev_id);

void rt_pend_linux_irq(unsigned irq);

RTAI_SYSCALL_MODE void usr_rt_pend_linux_irq(unsigned irq);

void rt_pend_linux_srq(unsigned srq);

int rt_request_srq(unsigned label,
		   void (*k_handler)(void),
		   long long (*u_handler)(unsigned long));

int rt_free_srq(unsigned srq);

int rt_assign_irq_to_cpu(int irq,
			 unsigned long cpus_mask);

int rt_reset_irq_to_sym_mode(int irq);

void rt_request_timer_cpuid(void (*handler)(void),
			    unsigned tick,
			    int cpuid);

int rt_request_timer(void (*handler)(void), unsigned tick, int);

void rt_free_timer(void);

RT_TRAP_HANDLER rt_set_trap_handler(RT_TRAP_HANDLER handler);

void rt_release_rtc(void);

void rt_request_rtc(long rtc_freq, void *handler);

#define rt_mount()

#define rt_umount()

void (*rt_set_ihook(void (*hookfn)(int)))(int);

/* Deprecated calls. */

static inline int rt_request_global_irq(unsigned irq, void (*handler)(void))
{
	return rt_request_irq(irq, (int (*)(unsigned,void *))handler, 0, 0);
}

static inline int rt_request_global_irq_ext(unsigned irq, void (*handler)(void), unsigned long cookie)
{
	return rt_request_irq(irq, (int (*)(unsigned,void *))handler, (void *)cookie, 1);
}

static inline void rt_set_global_irq_ext(unsigned irq, int ext, unsigned long cookie)
{
	rt_set_irq_cookie(irq, (void *)cookie);
}

static inline int rt_free_global_irq(unsigned irq)
{
	return rt_release_irq(irq);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __KERNEL__ */

#include <asm/rtai_oldnames.h>

#define RTAI_DEFAULT_TICK    100000
#ifdef CONFIG_RTAI_TRACE
#define RTAI_DEFAULT_STACKSZ 8192
#else /* !CONFIG_RTAI_TRACE */
#define RTAI_DEFAULT_STACKSZ 1024
#endif /* CONFIG_RTAI_TRACE */

/*@}*/

#endif /* !_RTAI_ASM_M68KNOMMU_HAL_H */


#ifndef _RTAI_HAL_XN_H
#define _RTAI_HAL_XN_H

// this is now a bit misplaced, to be moved where it should belong

#define SET_FUSION_TIMER_RUNNING()

#define CLEAR_FUSION_TIMER_RUNNING()

#define IS_FUSION_TIMER_RUNNING()  (0)

#define NON_RTAI_SCHEDULE(cpuid)  do { schedule(); } while (0)

#endif /* !_RTAI_HAL_XN_H */
