/**
 *   @ingroup hal
 *   @file
 *
 *   ARTI -- RTAI-compatible Adeos-based Real-Time Interface. Based on
 *   the original RTAI layer for PPC and the RTAI/x86 rewrite over ADEOS.
 *
 *   Original RTAI/PPC layer implementation: \n
 *   Copyright &copy; 2000 Paolo Mantegazza, \n
 *   Copyright &copy; 2001 David Schleef, \n
 *   Copyright &copy; 2001 Lineo, Inc, \n
 *   Copyright &copy; 2002 Wolfgang Grandegger. \n
 *
 *   RTAI/PPC rewrite over hal-linux patches: \n
 *   Copyright &copy 2006 Antonio Barbalace.
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


#ifndef _RTAI_ASM_PPC_HAL_H
#define _RTAI_ASM_PPC_HAL_H

#define RTAI_SYSCALL_MODE //__attribute__((regparm(0)))

#define LOCKED_LINUX_IN_IRQ_HANDLER
#define UNWRAPPED_CATCH_EVENT

#include <rtai_hal_names.h>
#include <asm/rtai_vectors.h>
#include <rtai_types.h>

#ifdef CONFIG_SMP
#define RTAI_NR_CPUS  CONFIG_RTAI_CPUS
#else /* !CONFIG_SMP */
#define RTAI_NR_CPUS  1
#endif /* CONFIG_SMP */

#define NETRPC_ALIGN_RTIME(i) (1 - i%2)

//---------------------------------------------------------------------------//
//                         Mathematical primitives                           //
//---------------------------------------------------------------------------//

static inline int ffnz(unsigned long ul)
{
	__asm__ __volatile__ ("cntlzw %0, %1" : "=r" (ul) : "r" (ul & (-ul)));
	return 31 - ul;
}

/* One of the silly thing of 32 bits PPCs, no 64 bits result for 32 bits mul. */
static inline unsigned long long rtai_ullmul(unsigned long m0, unsigned long m1)
{
	unsigned long long res;
	__asm__ __volatile__ ("mulhwu %0, %1, %2"
			       : "=r" (((unsigned long *)(void *)&res)[0])
			       : "%r" (m0), "r" (m1));
	((unsigned long *)(void *)&res)[1] = m0*m1;
	return res;
}

/* One of the silly thing of 32 bits PPCs, no 64 by 32 bits divide. */
static inline unsigned long long rtai_ulldiv(unsigned long long ull, unsigned long uld, unsigned long *r)
{
	unsigned long long q, rf;
	unsigned long qh, rh, ql, qf;

	q = 0;
	rf = (unsigned long long)(0xFFFFFFFF - (qf = 0xFFFFFFFF / uld) * uld) + 1ULL;
	while (ull >= uld) {
		((unsigned long *)(void *)&q)[0] += (qh = ((unsigned long *)(void *)&ull)[0] / uld);
		rh = ((unsigned long *)(void *)&ull)[0] - qh * uld;
		q += rh * (unsigned long long)qf + (ql = ((unsigned long *)(void *)&ull)[1] / uld);
		ull = rh * rf + (((unsigned long *)(void *)&ull)[1] - ql * uld);
	}
	*r = ull;
	return q;
}

static inline int rtai_imuldiv(int i, int mult, int div)
{
    /* Returns (int)i = (int)i*(int)(mult)/(int)div. */
	unsigned long q, r;
	q = rtai_ulldiv(rtai_ullmul(i, mult), div, &r);
	return (r + r) > div ? q + 1 : q;
}

static inline unsigned long long rtai_llimd(unsigned long long ull, unsigned long mult, unsigned long div)
{
    /* Returns (long long)ll = (int)ll*(int)(mult)/(int)div. */
	unsigned long long low;
	unsigned long q, r;

	low  = rtai_ullmul(((unsigned long *)(void *)&ull)[1], mult);	
	q = rtai_ulldiv(rtai_ullmul(((unsigned long *)(void *)&ull)[0], mult) + ((unsigned long *)(void *)&low)[0], div, (unsigned long *)(void *)&low);
	low = rtai_ulldiv(low, div, &r);
	((unsigned long *)(void *)&low)[0] += q;

	return (r + r) > div ? low + 1 : low;
}


//---------------------------------------------------------------------------//
//                     Synchronization primitives                            //
//---------------------------------------------------------------------------//

#if defined(__KERNEL__) && !defined(__cplusplus)
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/time.h>

#include <asm/rtai_atomic.h>
#include <asm/rtai_fpu.h>
#include <rtai_trace.h>

struct rtai_realtime_irq_s {
        int (*handler)(unsigned irq, void *cookie);
        void *cookie;
        int retmode;
        int cpumask;
        int (*irq_ack)(unsigned int, void *);
};

#define RTAI_DOMAIN_ID  0x52544149
#define RTAI_NR_TRAPS   HAL_NR_FAULTS
#define RTAI_NR_SRQS    32

#define RTAI_TIMER_DECR_IRQ       IPIPE_VIRQ_BASE
#define RTAI_TIMER_8254_IRQ       RTAI_TIMER_DECR_IRQ
#define RTAI_FREQ_DECR            (rtai_tunables.cpu_freq)
#define RTAI_FREQ_8254            (rtai_tunables.cpu_freq)
#define RTAI_LATENCY_8254         CONFIG_RTAI_SCHED_8254_LATENCY
#define RTAI_SETUP_TIME_8254      500

#define RTAI_TIME_LIMIT           0x7000000000000000LL

#define RTAI_IFLAG  15

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

#define RTAI_LT_KERNEL_VERSION_FOR_NONPERCPU  KERNEL_VERSION(2,6,20)

#if LINUX_VERSION_CODE < RTAI_LT_KERNEL_VERSION_FOR_NONPERCPU

#define ROOT_STATUS_ADR(cpuid)  (ipipe_root_status[cpuid])
#define ROOT_STATUS_VAL(cpuid)  (*ipipe_root_status[cpuid])

#define hal_pend_domain_uncond(irq, domain, cpuid) \
do { \
	hal_irq_hits_pp(irq, domain, cpuid); \
	if (likely(!test_bit(IPIPE_LOCK_FLAG, &(domain)->irqs[irq].control))) { \
		__set_bit((irq) & IPIPE_IRQ_IMASK, &(domain)->cpudata[cpuid].irq_pending_lo[(irq) >> IPIPE_IRQ_ISHIFT]); \
		__set_bit((irq) >> IPIPE_IRQ_ISHIFT, &(domain)->cpudata[cpuid].irq_pending_hi); \
	} \
} while (0)

#define hal_fast_flush_pipeline(cpuid) \
do { \
	if (hal_root_domain->cpudata[cpuid].irq_pending_hi != 0) { \
		rtai_cli(); \
		hal_sync_stage(IPIPE_IRQMASK_ANY); \
	} \
} while (0)

#else

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

#endif

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

struct calibration_data {
    unsigned long cpu_freq;
    unsigned long apic_freq;
    int latency;
    int setup_time_TIMER_CPUNIT;
    int setup_time_TIMER_UNIT;
    int timers_tol[RTAI_NR_CPUS];
};

extern struct rt_times rt_times;
extern struct rt_times rt_smp_times[RTAI_NR_CPUS];
extern struct calibration_data rtai_tunables;
extern volatile unsigned long rtai_cpu_realtime;
extern volatile unsigned long rtai_cpu_lock[];

#define SET_TASKPRI(cpuid)
#define CLR_TASKPRI(cpuid)

extern struct rtai_switch_data {
	volatile unsigned long sflags;
	volatile unsigned long lflags;
} rtai_linux_context[RTAI_NR_CPUS];

static inline unsigned long rtai_save_flags_irqbit(void)
{
	unsigned long flags;
	rtai_save_flags(flags);
	return flags & (1 << RTAI_IFLAG);
}

static inline unsigned long rtai_save_flags_irqbit_and_cli(void)
{
	unsigned long flags;
	rtai_save_flags_and_cli(flags);
	return flags & (1 << RTAI_IFLAG);
}

#ifdef CONFIG_SMP

#define SCHED_VECTOR  RTAI_SMP_NOTIFY_VECTOR
#define SCHED_IPI     RTAI_SMP_NOTIFY_IPI

#define _send_sched_ipi(dest)  do { mb(); mpic_send_ipi(0x2, dest); } while (0)

#ifdef CONFIG_PREEMPT
#define rt_spin_lock(lock)    do { barrier(); _raw_spin_lock(lock); barrier(); } while (0)
#define rt_spin_unlock(lock)  do { barrier(); _raw_spin_unlock(lock); barrier(); } while (0)
#else /* !CONFIG_PREEMPT */
#define rt_spin_lock(lock)    spin_lock(lock)
#define rt_spin_unlock(lock)  spin_unlock(lock)
#endif /* CONFIG_PREEMPT */

static inline void rt_spin_lock_hw_irq(spinlock_t *lock)
{
	rtai_hw_cli();
	rt_spin_lock(lock);
}

static inline void rt_spin_unlock_hw_irq(spinlock_t *lock)
{
	rt_spin_unlock(lock);
	rtai_hw_sti();
}

static inline unsigned long rt_spin_lock_hw_irqsave(spinlock_t *lock)
{
	unsigned long flags;
	rtai_hw_save_flags_and_cli(flags);
	rt_spin_lock(lock);
	return flags;
}

static inline void rt_spin_unlock_hw_irqrestore(unsigned long flags, spinlock_t *lock)
{
	rt_spin_unlock(lock);
	rtai_hw_restore_flags(flags);
}

static inline void rt_spin_lock_irq(spinlock_t *lock)
{
    rtai_cli();
    rt_spin_lock(lock);
}

static inline void rt_spin_unlock_irq(spinlock_t *lock)
{
    rt_spin_unlock(lock);
    rtai_sti();
}

static inline unsigned long rt_spin_lock_irqsave(spinlock_t *lock)
{
    unsigned long flags;
    rtai_save_flags_and_cli(flags);
    rt_spin_lock(lock);
    return flags;
}

static inline void rt_spin_unlock_irqrestore(unsigned long flags, spinlock_t *lock)
{
	rt_spin_unlock(lock);
	rtai_local_irq_restore(flags);
}

#if RTAI_NR_CPUS > 0

static inline void rtai_spin_glock(volatile unsigned long *lock)
{
	unsigned long val, owner;
#if 0
	do {
		val = lock[1];
	} while (cmpxchg(&lock[1], val, (val + 0x10000) & 0x7FFF7FFF) != val);
#else
	val = atomic_add_return(0x10000, (atomic_t *)&lock[1]) - 0x10000;
#endif
	if ((owner = (val & 0x7FFF0000) >> 16) != (val & 0x7FFF)) {
		while ((lock[1] & 0x7FFF) != owner) {
			 cpu_relax();
		}
	}
}

static inline void rtai_spin_gunlock(volatile unsigned long *lock)
{
	unsigned long val;
	do {
		val = lock[1];
		cpu_relax();
	} while (cmpxchg(&lock[1], val, (val + 1) & 0x7FFF7FFF) != val);
}

#else

static inline void rtai_spin_glock(volatile unsigned long *lock)
{
	while (test_and_set_bit(31, lock)) {
		cpu_relax();
	}
	barrier();
}

static inline void rtai_spin_gunlock(volatile unsigned long *lock)
{
	test_and_clear_bit(31, lock);
	cpu_relax();
}

#endif

static inline void rt_get_global_lock(void)
{
        barrier();
        rtai_cli();
        if (!test_and_set_bit(hal_processor_id(), &rtai_cpu_lock[0])) {
		rtai_spin_glock(&rtai_cpu_lock[0]);
        }
        barrier();
}

static inline void rt_release_global_lock(void)
{
        barrier();
        rtai_cli();
        if (test_and_clear_bit(hal_processor_id(), &rtai_cpu_lock[0])) {
		rtai_spin_gunlock(&rtai_cpu_lock[0]);
        }
        barrier();
}

/**
 * Disable interrupts across all CPUs
 *
 * rt_global_cli hard disables interrupts (cli) on the requesting CPU and
 * acquires the global spinlock to the calling CPU so that any other CPU
 * synchronized by this method is blocked. Nested calls to rt_global_cli within
 * the owner CPU will not cause a deadlock on the global spinlock, as it would
 * happen for a normal spinlock.
 *
 * rt_global_sti hard enables interrupts (sti) on the calling CPU and releases
 * the global lock.
 */
static inline void rt_global_cli(void)
{
    rt_get_global_lock();
}

/**
 * Enable interrupts across all CPUs
 *
 * rt_global_sti hard enables interrupts (sti) on the calling CPU and releases
 * the global lock.
 */
static inline void rt_global_sti(void)
{
    rt_release_global_lock();
    rtai_sti();
}

/**
 * Save CPU flags
 *
 * rt_global_save_flags_and_cli combines rt_global_save_flags() and
 * rt_global_cli().
 */
static inline int rt_global_save_flags_and_cli(void)
{
        unsigned long flags;

        barrier();
        flags = rtai_save_flags_irqbit_and_cli();
        if (!test_and_set_bit(hal_processor_id(), &rtai_cpu_lock[0])) {
		rtai_spin_glock(&rtai_cpu_lock[0]);
                barrier();
                return flags | 1;
        }
        barrier();
        return flags;
}

/**
 * Save CPU flags
 *
 * rt_global_save_flags saves the CPU interrupt flag (IF) bit 9 of @a flags and
 * ORs the global lock flag in the first 8 bits of flags. From that you can
 * rightly infer that RTAI does not support more than 8 CPUs.
 */
static inline void rt_global_save_flags(unsigned long *flags)
{
        unsigned long hflags = rtai_save_flags_irqbit_and_cli();

        *flags = test_bit(hal_processor_id(), &rtai_cpu_lock[0]) ? hflags : hflags | 1;
        if (hflags) {
                rtai_sti();
        }
}

/**
 * Restore CPU flags
 *
 * rt_global_restore_flags restores the CPU hard interrupt flag (IF)
 * and the state of the global inter-CPU lock, according to the state
 * given by flags.
 */
static inline void rt_global_restore_flags(unsigned long flags)
{
        barrier();
	if (test_and_clear_bit(0, &flags)) {
		rt_release_global_lock();
	} else {
		rt_get_global_lock();
	}
	if (flags) {
		rtai_sti();
	}
        barrier();
}

#else /* !CONFIG_SMP */

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

#endif /* CONFIG_SMP */


//---------------------------------------------------------------------------//
//                     domain switching routines                             //
//---------------------------------------------------------------------------//

extern struct hal_domain_struct rtai_domain;

#define _rt_switch_to_real_time(cpuid) \
do { \
	rtai_linux_context[cpuid].lflags = xchg((unsigned long *)ROOT_STATUS_ADR(cpuid), (1 << IPIPE_STALL_FLAG)); \
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

#define rtai_get_intr_handler(v)
#define rtai_init_taskpri_irqs()

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

//---------------------------------------------------------------------------//
//                        Timer helper functions                             //
//---------------------------------------------------------------------------//

static inline unsigned long long rtai_rdtsc (void)
{
	unsigned long long ts;
	unsigned long chk;
	/* See Motorola reference manual for 32 bits PPCs. */
	__asm__ __volatile__ ("1: mftbu %0\n"
			      "   mftb %1\n"
			      "   mftbu %2\n"
			      "   cmpw %2,%0\n"
			      "   bne 1b\n"
			      : "=r" (((unsigned long *)&ts)[0]),
			        "=r" (((unsigned long *)&ts)[1]),
			        "=r" (chk));
	return ts;
}

static inline void rt_set_timer_delay (int delay)
{
    /* NOTE: delay MUST be 0 if a periodic timer is being used. */
	if (delay == 0) {
#ifdef CONFIG_40x
		return;
#else  /* !CONFIG_40x */
		while ((delay = rt_times.intr_time - rtai_rdtsc()) <= 0) {
			rt_times.intr_time += rt_times.periodic_tick;
		}
#endif /* CONFIG_40x */
	}
#ifdef CONFIG_40x
	mtspr(SPRN_PIT, delay);
#else /* !CONFIG_40x */
	set_dec(delay);
#endif /* CONFIG_40x */
}

static inline void rtai_disarm_decr(int cpuid, int mode)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23)
	per_cpu(disarm_decr, cpuid) = mode;
#else
	disarm_decr[cpuid] = mode;
#endif
}

//---------------------------------------------------------------------------//
//                     Private interface -- internal use only                //
//---------------------------------------------------------------------------//

unsigned long rtai_critical_enter(void (*synch)(void));
void rtai_critical_exit(unsigned long flags);

int rtai_calibrate_8254(void);
void rtai_set_linux_task_priority(struct task_struct *task, int policy, int prio);
long rtai_catch_event (struct hal_domain_struct *ipd, unsigned long event, int (*handler)(unsigned long, void *));

#endif /* __KERNEL__ && !__cplusplus */


//---------------------------------------------------------------------------//
//                           Public interface                                //
//---------------------------------------------------------------------------//

struct apic_timer_setup_data {
    int mode;
    int count;
};

#ifdef __KERNEL__

#include <linux/kernel.h>

#define rtai_print_to_screen rt_printk

void *ll2a(long long ll, char *s);

#ifdef __cplusplus
 extern "C" {
#endif /* __cplusplus */

/*
 * rt irq request/free modify functions
 */
int rt_request_irq(unsigned irq, int (*handler)(unsigned irq, void *cookie), void *cookie, int retmode);
int rt_release_irq(unsigned irq);
int rt_set_irq_ack(unsigned irq, int (*irq_ack)(unsigned int));
void rt_set_irq_cookie(unsigned irq, void *cookie);
void rt_set_irq_retmode(unsigned irq, int retmode);

static inline int rt_request_irq_wack(unsigned irq, int (*handler)(unsigned irq, void *cookie), void *cookie, int retmode, int (*irq_ack)(unsigned int))
{
	int retval;
	if ((retval = rt_request_irq(irq, handler, cookie, retmode)) < 0) {
		return retval;
	}
	return rt_set_irq_ack(irq, irq_ack);
}


/*
 * irq/PIC management functions.
 */

unsigned rt_startup_irq(unsigned irq);
void rt_shutdown_irq(unsigned irq);

void rt_enable_irq(unsigned irq);
void rt_disable_irq(unsigned irq);
void rt_mask_and_ack_irq(unsigned irq);
void rt_unmask_irq(unsigned irq);
void rt_ack_irq(unsigned irq);
void rt_end_irq(unsigned irq);


/*
 * Linux related irq function
 */

int rt_request_linux_irq(unsigned irq, void *handler, char *name, void *dev_id);
int rt_free_linux_irq(unsigned irq, void *dev_id);
void rt_pend_linux_irq(unsigned irq);
RTAI_SYSCALL_MODE void usr_rt_pend_linux_irq(unsigned irq);
void rtai_set_linux_task_priority(struct task_struct *task, int policy, int prio);

/*
 * srq related function
 */

void rt_pend_linux_srq(unsigned srq);
int rt_request_srq (unsigned label, void (*k_handler)(void), long long (*u_handler)(unsigned long));
int rt_free_srq(unsigned srq);

int rt_assign_irq_to_cpu(int irq, unsigned long cpus_mask);
int rt_reset_irq_to_sym_mode(int irq);
void rt_request_timer_cpuid(void (*handler)(void), unsigned tick, int cpuid);

/*
 * timer func
 */

int rt_request_timer(void (*handler)(void), unsigned tick, int use_apic);
void rt_free_timer(void);

void rt_request_rtc(long rtc_freq, void *handler);
void rt_release_rtc(void);

/*
 * setting trap/hook handler function
 */

RT_TRAP_HANDLER rt_set_trap_handler(RT_TRAP_HANDLER handler);
void (*rt_set_ihook(void (*hookfn)(int)))(int);

RTIME rd_8254_ts(void);
void rt_setup_8254_tsc(void);

/*
 * real time printk
 */

int rt_printk(const char *format, ...);
int rt_sync_printk(const char *format, ...);

#ifdef __cplusplus
 }
#endif /* __cplusplus */

#endif /* __KERNEL__ */

#include <asm/rtai_oldnames.h>

#define RTAI_DEFAULT_TICK    100000

#ifdef CONFIG_RTAI_TRACE
#define RTAI_DEFAULT_STACKSZ 8192
#else /* !CONFIG_RTAI_TRACE */
#define RTAI_DEFAULT_STACKSZ 4092
#endif /* CONFIG_RTAI_TRACE */

#endif /* _RTAI_ASM_PPC_HAL_H */

#ifndef _RTAI_HAL_XN_H
#define _RTAI_HAL_XN_H

#define __range_ok(addr, size) (__range_not_ok(addr,size) == 0)

#define NON_RTAI_SCHEDULE(cpuid)  do { schedule(); } while (0)

#endif /* !_RTAI_HAL_XN_H */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)

#ifndef _ASM_GENERIC_DIV64_H
#define _ASM_GENERIC_DIV64_H
/*
 * Copyright (C) 2003 Bernardo Innocenti <bernie@develer.com>
 * Based on former asm-ppc/div64.h and asm-m68knommu/div64.h
 *
 * The semantics of do_div() are:
 *
 * uint32_t do_div(uint64_t *n, uint32_t base)
 * {
 * 	uint32_t remainder = *n % base;
 * 	*n = *n / base;
 * 	return remainder;
 * }
 *
 * NOTE: macro parameter n is evaluated multiple times,
 *       beware of side effects!
 */

//#include <linux/types.h>
//#include <linux/compiler.h>

#if BITS_PER_LONG == 64

# define do_div(n,base) ({					\
	uint32_t __base = (base);				\
	uint32_t __rem;						\
	__rem = ((uint64_t)(n)) % __base;			\
	(n) = ((uint64_t)(n)) / __base;				\
	__rem;							\
 })

#elif BITS_PER_LONG == 32

extern uint32_t __div64_32(uint64_t *dividend, uint32_t divisor);

/* The unnecessary pointer compare is there
 * to check for type safety (n must be 64bit)
 */
# define do_div(n,base) ({				\
	uint32_t __base = (base);			\
	uint32_t __rem;					\
	(void)(((typeof((n)) *)0) == ((uint64_t *)0));	\
	if (likely(((n) >> 32) == 0)) {			\
		__rem = (uint32_t)(n) % __base;		\
		(n) = (uint32_t)(n) / __base;		\
	} else 						\
		__rem = __div64_32(&(n), __base);	\
	__rem;						\
 })

#endif /* BITS_PER_LONG */

#endif /* _ASM_GENERIC_DIV64_H */

#endif
