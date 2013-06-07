/**
 *   @ingroup hal
 *   @file
 *
 *   ARTI -- RTAI-compatible Adeos-based Real-Time Interface. Based on
 *   the original RTAI layer for PPC and the RTAI/x86 rewrite over ADEOS.
 *
 *   Original RTAI/PPC layer implementation: \n
 *   Copyright &copy; 2000-2007 Paolo Mantegazza, \n
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


#include <linux/version.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/console.h>

#include <asm/system.h>
#include <asm/hw_irq.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mmu_context.h>
#include <asm/uaccess.h>
#include <asm/time.h>
#include <asm/types.h>
#include <asm/machdep.h>

#define __RTAI_HAL__
#include <asm/rtai_hal.h>
#include <asm/rtai_lxrt.h>


#ifdef CONFIG_PROC_FS
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <rtai_proc_fs.h>
#endif /* CONFIG_PROC_FS */

#include <stdarg.h>

/* kernel module tricks */
MODULE_LICENSE("GPL");

#define INTR_VECTOR  5
#define DECR_VECTOR  9

static unsigned long rtai_cpufreq_arg = RTAI_CALIBRATED_CPU_FREQ;
RTAI_MODULE_PARM(rtai_cpufreq_arg, ulong);

#define RTAI_NR_IRQS  IPIPE_NR_XIRQS

static int PrintFpuTrap = 0;
RTAI_MODULE_PARM(PrintFpuTrap, int);
static int PrintFpuInit = 0;
RTAI_MODULE_PARM(PrintFpuInit, int);
unsigned long IsolCpusMask = 0;
RTAI_MODULE_PARM(IsolCpusMask, ulong);

struct { volatile int locked, rqsted; } rt_scheduling[RTAI_NR_CPUS];

#ifdef CONFIG_RTAI_SCHED_ISR_LOCK
static void (*rtai_isr_hook)(int cpuid);
#endif /* CONFIG_RTAI_SCHED_ISR_LOCK */

#ifdef CONFIG_RTAI_SCHED_ISR_LOCK
#define RTAI_SCHED_ISR_LOCK() \
	do { \
		if (!rt_scheduling[cpuid].locked++) { \
			rt_scheduling[cpuid].rqsted = 0; \
		} \
	} while (0)
#define RTAI_SCHED_ISR_UNLOCK() \
	do { \
		if (rt_scheduling[cpuid].locked && !(--rt_scheduling[cpuid].locked)) { \
			if (rt_scheduling[cpuid].rqsted > 0 && rtai_isr_hook) { \
				rtai_isr_hook(cpuid); \
        		} \
		} \
	} while (0)
#else /* !CONFIG_RTAI_SCHED_ISR_LOCK */
#define RTAI_SCHED_ISR_LOCK() \
	do { cpuid = rtai_cpuid(); } while (0)
#define RTAI_SCHED_ISR_UNLOCK() \
	do {                       } while (0)
#endif /* CONFIG_RTAI_SCHED_ISR_LOCK */

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,31) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9)
#define HAL_TICK_REGS hal_tick_regs[cpuid]
#else
#define HAL_TICK_REGS hal_tick_regs
#endif /* (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,31) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)) || LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9) */

#ifdef LOCKED_LINUX_IN_IRQ_HANDLER
#define HAL_LOCK_LINUX()  do { sflags = rt_save_switch_to_real_time(cpuid = rtai_cpuid()); } while (0)
#define HAL_UNLOCK_LINUX()  do { rtai_cli(); rt_restore_switch_to_linux(sflags, cpuid); } while (0)
#else
#define HAL_LOCK_LINUX()  do { sflags = xchg((unsigned long *)ROOT_STATUS_ADR(cpuid), (1 << IPIPE_STALL_FLAG)); } while (0)
#define HAL_UNLOCK_LINUX()  do { rtai_cli(); ROOT_STATUS_VAL(cpuid) = sflags; } while (0)
#endif /* LOCKED_LINUX_IN_IRQ_HANDLER */

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,26)

#define RTAI_IRQ_ACK(irq) \
	do { \
		rtai_realtime_irq[irq].irq_ack(irq, irq_desc + irq); \
	} while (0)

#else

#define RTAI_IRQ_ACK(irq) \
	do { \
		((void (*)(unsigned int))rtai_realtime_irq[irq].irq_ack)(irq); \
	} while (0)

#endif

#define CHECK_KERCTX()

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#define rtai_irq_desc(irq) (irq_desc[irq].handler)
#else
#define rtai_irq_desc(irq) (irq_desc[irq].chip)
#endif

#define BEGIN_PIC()
#define END_PIC()
#undef hal_lock_irq
#undef hal_unlock_irq
#define hal_lock_irq(x, y, z)
#define hal_unlock_irq(x, y)

#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11) */

extern struct hw_interrupt_type hal_std_irq_dtype[];
#define rtai_irq_desc(irq) (&hal_std_irq_dtype[irq])

#define BEGIN_PIC() \
do { \
        unsigned long flags, pflags, cpuid; \
	rtai_save_flags_and_cli(flags); \
	cpuid = rtai_cpuid(); \
	pflags = xchg((unsigned long *)ROOT_STATUS_ADR(cpuid), 1 << IPIPE_STALL_FLAG); \
	rtai_save_and_lock_preempt_count()

#define END_PIC() \
	rtai_restore_preempt_count(); \
	ROOT_STATUS_VAL(cpuid) = pflags; \
	rtai_restore_flags(flags); \
} while (0)

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11) */


/* global var section */
static atomic_t rtai_sync_count = ATOMIC_INIT(1);
static volatile int rtai_sync_level;
static unsigned rtai_sysreq_virq;
struct rtai_realtime_irq_s rtai_realtime_irq[RTAI_NR_IRQS];  // declared in base/include/asm-ppc/rtai_hal.h

#if LINUX_VERSION_CODE < RTAI_LT_KERNEL_VERSION_FOR_NONPERCPU
volatile unsigned long *ipipe_root_status[RTAI_NR_CPUS];
#endif

struct calibration_data rtai_tunables;

extern void *hal_syscall_handler;

static RT_TRAP_HANDLER rtai_trap_handler;
extern struct machdep_calls ppc_md;
static unsigned rtai_sysreq_virq;
static unsigned long rtai_sysreq_map = 1;   /* srq 0 is reserved */
static unsigned long rtai_sysreq_pending;
static unsigned long rtai_sysreq_running;
static spinlock_t rtai_lsrq_lock = SPIN_LOCK_UNLOCKED;

static struct {
	unsigned long flags;
	int count;
} rtai_linux_irq[RTAI_NR_IRQS];

static struct {
	void (*k_handler)(void);
	long long (*u_handler)(unsigned long);
	unsigned long label;
} rtai_sysreq_table[RTAI_NR_SRQS];

volatile unsigned long rtai_cpu_lock[2];
struct hal_domain_struct rtai_domain;
volatile unsigned long rtai_cpu_realtime;
struct rt_times rt_times;
struct rtai_switch_data rtai_linux_context[RTAI_NR_CPUS];
struct rt_times rt_smp_times[RTAI_NR_CPUS];


/*
 * rtai_critical_enter
 */

unsigned long rtai_critical_enter (void (*synch)(void))
{
	unsigned long flags;

	flags = hal_critical_enter(synch);
	if (atomic_dec_and_test(&rtai_sync_count)) {
		rtai_sync_level = 0;
	} else if (synch != NULL) {
		printk(KERN_INFO "RTAI[hal]: warning: nested sync will fail.\n");
	}
	return flags;
}


/*
 * rtai_critical_exit
 */

void rtai_critical_exit (unsigned long flags)
{
	atomic_inc(&rtai_sync_count);
	hal_critical_exit(flags);
}


/*
 * rt_request_irq
 */

int rt_request_irq(unsigned irq, int (*handler)(unsigned irq, void *cookie), void *cookie, int retmode)
{
	unsigned long flags;

	if (handler == NULL || irq >= RTAI_NR_IRQS) {
		return -EINVAL;
	}
	if (rtai_realtime_irq[irq].handler != NULL) {
		return -EBUSY;
	}

	flags = rtai_critical_enter(NULL);
	rtai_realtime_irq[irq].handler = (void *)handler;
	rtai_realtime_irq[irq].cookie  = cookie;
	rtai_realtime_irq[irq].retmode = retmode ? 1 : 0;
	rtai_realtime_irq[irq].irq_ack = (void *)hal_root_domain->irqs[irq].acknowledge;
	rtai_critical_exit(flags);

	if (IsolCpusMask && irq < IPIPE_NR_XIRQS) {
		rtai_realtime_irq[irq].cpumask = rt_assign_irq_to_cpu(irq, IsolCpusMask);
	}

	return 0;
}


/*
 * rt_release_irq
 */

int rt_release_irq (unsigned irq)
{
	unsigned long flags;
	if (irq >= RTAI_NR_IRQS || !rtai_realtime_irq[irq].handler) {
		return -EINVAL;
	}

	flags = rtai_critical_enter(NULL);
	rtai_realtime_irq[irq].handler = NULL;
	rtai_realtime_irq[irq].irq_ack = (void *)hal_root_domain->irqs[irq].acknowledge;
	rtai_critical_exit(flags);

	if (IsolCpusMask && irq < IPIPE_NR_XIRQS) {
		rt_assign_irq_to_cpu(irq, rtai_realtime_irq[irq].cpumask);
	}

	return 0;
}


/*
 * rt_set_irq_ack
 */

int rt_set_irq_ack(unsigned irq, int (*irq_ack)(unsigned int))
{
	if (irq >= RTAI_NR_IRQS) {
		return -EINVAL;
	}
	rtai_realtime_irq[irq].irq_ack = irq_ack ? irq_ack : (void *)hal_root_domain->irqs[irq].acknowledge;
	return 0;
}


/*
 * rt_set_irq_cookie
 */

void rt_set_irq_cookie (unsigned irq, void *cookie)
{
	if (irq < RTAI_NR_IRQS) {
		rtai_realtime_irq[irq].cookie = cookie;
	}
}


/*
 * rt_set_irq_retmode
 */

void rt_set_irq_retmode (unsigned irq, int retmode)
{
	if (irq < RTAI_NR_IRQS) {
		rtai_realtime_irq[irq].retmode = retmode ? 1 : 0;
	}
}


/*
 * rt_startup_irq
 */

unsigned rt_startup_irq (unsigned irq)
{
        int retval;

	BEGIN_PIC();
	hal_unlock_irq(hal_root_domain, irq);
	retval = rtai_irq_desc(irq)->startup(irq);
	END_PIC();
        return retval;
}


/*
 * rt_shutdown_irq
 */

void rt_shutdown_irq (unsigned irq)
{
	BEGIN_PIC();
	rtai_irq_desc(irq)->shutdown(irq);
#if LINUX_VERSION_CODE < RTAI_LT_KERNEL_VERSION_FOR_NONPERCPU
	hal_clear_irq(hal_root_domain, irq);
#endif
	END_PIC();
}

/*
 * _rt_enable_irq
 */

static inline void _rt_enable_irq (unsigned irq)
{
	BEGIN_PIC();
	hal_unlock_irq(hal_root_domain, irq);
	rtai_irq_desc(irq)->enable(irq);
	END_PIC();
}


/*
 * rt_disable_irq
 */

void rt_disable_irq (unsigned irq)
{
	BEGIN_PIC();
	rtai_irq_desc(irq)->disable(irq);
	hal_lock_irq(hal_root_domain, cpuid, irq);
	END_PIC();
}


/*
 * _rt_end_irq
 */

static inline void _rt_end_irq (unsigned irq)
{
	BEGIN_PIC();
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS))) {
		hal_unlock_irq(hal_root_domain, irq);
	}
	rtai_irq_desc(irq)->end(irq);
	END_PIC();
}


// rt_mask_and_ack_irq
void rt_mask_and_ack_irq (unsigned irq) { rtai_irq_desc(irq)->ack(irq); }
// rt_enable_irq
void rt_enable_irq (unsigned irq) { _rt_enable_irq(irq); }
// rt_mask_irq
void rt_unmask_irq (unsigned irq) { _rt_end_irq(irq); }
// rt_ack_irq
void rt_ack_irq (unsigned irq) { _rt_enable_irq(irq); }
// rt_end_irq
void rt_end_irq (unsigned irq) { _rt_end_irq(irq); }


/*
 * rt_request_linux_irq
 */

int rt_request_linux_irq (unsigned irq, void *handler, char *name, void *dev_id)
{
	unsigned long flags;
	int retval;

	if (irq >= RTAI_NR_IRQS || !handler) {
		return -EINVAL;
	}

	rtai_save_flags_and_cli(flags);
	spin_lock(&irq_desc[irq].lock);
	if (rtai_linux_irq[irq].count++ == 0 && irq_desc[irq].action) {
		rtai_linux_irq[irq].flags = irq_desc[irq].action->flags;
		irq_desc[irq].action->flags |= IRQF_SHARED;
	}
	spin_unlock(&irq_desc[irq].lock);
	rtai_restore_flags(flags);

	retval = request_irq(irq, handler, IRQF_SHARED, name, dev_id);

	return 0;
}


/*
 * rt_free_linux_irq
 */

int rt_free_linux_irq (unsigned irq, void *dev_id)
{
	unsigned long flags;

	if (irq >= RTAI_NR_IRQS || rtai_linux_irq[irq].count == 0) {
		return -EINVAL;
	}

	rtai_save_flags_and_cli(flags);
	free_irq(irq, dev_id);

	spin_lock(&irq_desc[irq].lock);
	if (--rtai_linux_irq[irq].count == 0 && irq_desc[irq].action) {
		irq_desc[irq].action->flags = rtai_linux_irq[irq].flags;
	}
	spin_unlock(&irq_desc[irq].lock);

	rtai_restore_flags(flags);

	return 0;
}


/*
 * rt_pend_linux_irq
 */

void rt_pend_linux_irq (unsigned irq)
{
	unsigned long flags;
	rtai_save_flags_and_cli(flags);
	hal_pend_uncond(irq, rtai_cpuid());
	rtai_restore_flags(flags);
}


/*
 * usr_rt_pend_linux_irq
 */

RTAI_SYSCALL_MODE void usr_rt_pend_linux_irq (unsigned irq)
{
	unsigned long flags;
	rtai_save_flags_and_cli(flags);
	hal_pend_uncond(irq, rtai_cpuid());
	rtai_restore_flags(flags);
}


/*
 * rt_request_srq
 */

int rt_request_srq (unsigned label, void (*k_handler)(void), long long (*u_handler)(unsigned long))
{
	unsigned long flags;
	int srq;

	if (k_handler == NULL) {
		return -EINVAL;
	}

	rtai_save_flags_and_cli(flags);

	if (rtai_sysreq_map != ~0) {
		set_bit(srq = ffz(rtai_sysreq_map), &rtai_sysreq_map);
		rtai_sysreq_table[srq].k_handler = k_handler;
		rtai_sysreq_table[srq].u_handler = u_handler;
		rtai_sysreq_table[srq].label = label;
	} else {
		srq = -EBUSY;
	}
	rtai_restore_flags(flags);

	return srq;
}


/*
 * rt_free_srq
 */

int rt_free_srq (unsigned srq)
{
	return  (srq < 1 || srq >= RTAI_NR_SRQS || !test_and_clear_bit(srq, &rtai_sysreq_map)) ? -EINVAL : 0;
}


/*
 * rt_pend_linux_srq
 */

void rt_pend_linux_srq (unsigned srq)
{
	if (srq > 0 && srq < RTAI_NR_SRQS) {
		unsigned long flags;
		set_bit(srq, &rtai_sysreq_pending);
		rtai_save_flags_and_cli(flags);
		hal_pend_uncond(rtai_sysreq_virq, rtai_cpuid());
		rtai_restore_flags(flags);
	}
}

#define NR_EXCEPT 48

struct intercept_entry { unsigned long handler, rethandler; };
extern struct intercept_entry *intercept_table[];
static struct intercept_entry old_intercept_table[NR_EXCEPT];

/*
 * rtai_set_gate_vector (more correctly rtai_set_trap_vector)
 */

struct intercept_entry rtai_set_gate_vector(unsigned vector, void *handler, void *rethandler)
{
	old_intercept_table[vector].handler    = intercept_table[vector]->handler;
	old_intercept_table[vector].rethandler = intercept_table[vector]->rethandler;
	if (handler) {
		intercept_table[vector]->handler    = (unsigned long)handler;
	}
	if (rethandler) {
		intercept_table[vector]->rethandler = (unsigned long)rethandler;
	}
	return old_intercept_table[vector];
}

/*
 * rtai_reset_gate_vector (rtai_reset_trap_vector)
 */

void rtai_reset_gate_vector (unsigned vector, unsigned long handler, unsigned long rethandler)
{
	if (!((handler | old_intercept_table[vector].handler) && (rethandler | old_intercept_table[vector].rethandler))) {
		return;
	}
	intercept_table[vector]->handler    = handler    ? handler    : old_intercept_table[vector].handler;
	intercept_table[vector]->rethandler = rethandler ? rethandler : old_intercept_table[vector].rethandler;
}

static void (*decr_timer_handler)(void);

/* this can be a prototype for a handler pending something for Linux */
int rtai_decr_timer_handler(struct pt_regs *regs)
{
	unsigned long cpuid;
	unsigned long sflags;

	HAL_LOCK_LINUX();
	RTAI_SCHED_ISR_LOCK();
	decr_timer_handler();
	RTAI_SCHED_ISR_UNLOCK();
	HAL_UNLOCK_LINUX();
	if (!test_bit(IPIPE_STALL_FLAG, ROOT_STATUS_ADR(cpuid))) {
		rtai_sti();
		hal_fast_flush_pipeline(cpuid);
		return 1;
	}
	return 0;
}


/*
 * Upgrade this function for SMP systems
 */

void rt_request_apic_timers (void (*handler)(void), struct apic_timer_setup_data *tmdata) { return; }
void rt_free_apic_timers(void) { rt_free_timer(); }
int rt_assign_irq_to_cpu (int irq, unsigned long cpus_mask) { return 0; }
int rt_reset_irq_to_sym_mode (int irq) { return 0; }


static int rtai_request_tickdev(void);

static void rtai_release_tickdev(void);

/*
 * rt_request_timer
 */

int rt_request_timer (void (*handler)(void), unsigned tick, int use_apic)
{
	unsigned long flags;

	rtai_save_flags_and_cli(flags);

	// read tick values: current time base register and linux tick
	rt_times.tick_time = rtai_rdtsc();
	rt_times.linux_tick = tb_ticks_per_jiffy;
    	if (tick > 0) { // periodic Mode
		// if tick is greater than tb_ticks_per_jiffy schedule a linux timer first
		if (tick > tb_ticks_per_jiffy) {
			tick = tb_ticks_per_jiffy;
		}
		rt_times.intr_time = rt_times.tick_time + tick;
		rt_times.linux_time = rt_times.tick_time + rt_times.linux_tick;
		rt_times.periodic_tick = tick;
#ifdef CONFIG_40x
		/* Set the PIT auto-reload mode */
		mtspr(SPRN_TCR, mfspr(SPRN_TCR) | TCR_ARE);
		/* Set the PIT reload value and just let it run. */
		mtspr(SPRN_PIT, tick);
#endif /* CONFIG_40x */
	} else { //one-shot Mode
		// in this mode we set all to decade at linux_tick
		rt_times.intr_time = rt_times.tick_time + rt_times.linux_tick;
		rt_times.linux_time = rt_times.tick_time + rt_times.linux_tick;
		rt_times.periodic_tick = rt_times.linux_tick;
#ifdef CONFIG_40x
		/* Disable the PIT auto-reload mode */
		mtspr(SPRN_TCR, mfspr(SPRN_TCR) & ~TCR_ARE);
#endif /* CONFIG_40x */
	}

	// request an IRQ and register it
	rt_release_irq(RTAI_TIMER_DECR_IRQ);
	decr_timer_handler = handler;

	// pass throught ipipe: register immediate timer_trap handler
	// on i386 for a periodic mode is rt_set_timer_delay(tick); -> is set rate generator at tick; in one shot set LATCH all for the 8254 timer. Here is the same.
	rtai_disarm_decr(rtai_cpuid(), 1);
	rt_set_timer_delay(rt_times.periodic_tick);
	rtai_set_gate_vector(DECR_VECTOR, rtai_decr_timer_handler, 0);

	rtai_request_tickdev();
	rtai_restore_flags(flags);
	return 0;
}


/*
 * rt_free_timer
 */

void rt_free_timer (void)
{
	unsigned long flags;

	rtai_save_flags_and_cli(flags);
	rtai_release_tickdev();
#ifdef CONFIG_40x
	/* Re-enable the PIT auto-reload mode */
	mtspr(SPRN_TCR, mfspr(SPRN_TCR) | TCR_ARE);
	/* Set the PIT reload value and just let it run. */
	mtspr(SPRN_PIT, tb_ticks_per_jiffy);
#endif /* CONFIG_40x */
	rtai_reset_gate_vector(DECR_VECTOR, 0, 0);
	rtai_disarm_decr(rtai_cpuid(), 0);
	rtai_restore_flags(flags);
}

void rt_request_rtc(long rtc_freq, void *handler)
{
	rt_printk("*** RTC NOT IMPLEMENTED YET ON THIS ARCH ***\n");
}

void rt_release_rtc(void)
{
	rt_printk("*** RTC NOT IMPLEMENTED YET ON THIS ARCH ***\n");
}


/*
 * rtai_hirq_dispatcher
 */

static int spurious_interrupts;

static int rtai_hirq_dispatcher(struct pt_regs *regs)
{
	unsigned long cpuid;
	int irq;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,14)
	if ((irq = ppc_md.get_irq()) >= RTAI_NR_IRQS) {
#else
	if ((irq = ppc_md.get_irq(regs)) >= RTAI_NR_IRQS) {
#endif
		spurious_interrupts++;
		return 0;
	}

	if (rtai_realtime_irq[irq].handler) {
		unsigned long sflags;

		HAL_LOCK_LINUX();
		RTAI_IRQ_ACK(irq);
//		rtai_realtime_irq[irq].irq_ack(irq); mb();
		RTAI_SCHED_ISR_LOCK();
		rtai_realtime_irq[irq].handler(irq, rtai_realtime_irq[irq].cookie);
		RTAI_SCHED_ISR_UNLOCK();
		HAL_UNLOCK_LINUX();

		if (rtai_realtime_irq[irq].retmode || test_bit(IPIPE_STALL_FLAG, ROOT_STATUS_ADR(cpuid))) {
			return 0;
		}
	} else {
		unsigned long lflags;
		lflags = xchg((unsigned long *)ROOT_STATUS_ADR(cpuid = rtai_cpuid()), (1 << IPIPE_STALL_FLAG));
		RTAI_IRQ_ACK(irq);
//		rtai_realtime_irq[irq].irq_ack(irq); mb();
		hal_pend_uncond(irq, cpuid);
		ROOT_STATUS_VAL(cpuid) = lflags;
		if (test_bit(IPIPE_STALL_FLAG, &lflags)) {
			return 0;
		}
	}
	rtai_sti();
	hal_fast_flush_pipeline(cpuid);
	return 1;
}


/*
 * rt_set_trap_handler
 */

RT_TRAP_HANDLER rt_set_trap_handler (RT_TRAP_HANDLER handler)
{
	return (RT_TRAP_HANDLER)xchg(&rtai_trap_handler, handler);
}


/*
 * rtai_trap_fault
 */

static int rtai_trap_fault (unsigned event, void *evdata)
{
#ifdef HINT_DIAG_TRAPS
	static unsigned long traps_in_hard_intr = 0;
        do {
                unsigned long flags;
                rtai_save_flags_and_cli(flags);
                if (!test_bit(RTAI_IFLAG, &flags)) {
                        if (!test_and_set_bit(event, &traps_in_hard_intr)) {
                                HINT_DIAG_MSG(rt_printk("TRAP %d HAS INTERRUPT DISABLED (TRAPS PICTURE %lx).\n", event, traps_in_hard_intr););
                        }
                }
        } while (0);
#endif /* HINT_DIAG_TRAPS */

	static const int trap2sig[] = {
		SIGSEGV,	// 0 - Data or instruction access exception
		SIGBUS,		// 1 - Alignment exception
		SIGFPE,		// 2 - Altivec unavailable
		SIGFPE,		// 3 - Program check exception
		SIGFPE,		// 4 - Machine check exception
		SIGFPE,		// 5 - Unknown exception
		SIGTRAP,	// 6 - Instruction breakpoint
		SIGFPE,		// 7 - Run mode exception
		SIGTRAP,	// 8 - Single-step exception
		SIGSEGV,	// 9 - Non-recoverable exception
		SIGILL,		// 10 - Software emulation
		SIGTRAP,	// 11 - Debug exception
		SIGSEGV,	// 12 - SPE exception
		SIGFPE,		// 13 - Altivec assist exception
		0,		// 14
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0
	};

	TRACE_RTAI_TRAP_ENTRY(evdata->event, 0);

	if (!in_hrt_mode(rtai_cpuid())) {
		goto propagate;
	}

	if (event == 2) { /* if Altivec unavailable */
/*
		rtai_hw_cli(); // in task context, so we can be preempted
		if (lnxtsk_uses_fpu(linux_task)) {
			restore_fpu(linux_task);
			if (PrintFpuTrap) {
				rt_printk("\nWARNING: FPU TRAP FROM HARD PID = %d\n", linux_task->pid);
			}
		} else {
			init_hard_fpu(linux_task);
			if (PrintFpuInit) {
				rt_printk("\nWARNING: FPU INITIALIZATION FROM HARD PID = %d\n", linux_task->pid);
			}
		}
		rtai_hw_sti();*/
		goto endtrap;
	}

	// if a trap handler is set call it
	if (rtai_trap_handler && rtai_trap_handler(event, trap2sig[event], (struct pt_regs *)evdata, NULL)) {
		goto endtrap;
	}

propagate:
	return 0;

endtrap:
	TRACE_RTAI_TRAP_EXIT();
	return 1;
}


/*
 * rtai_lsrq_dispatcher
 */

static void rtai_lsrq_dispatcher (unsigned virq)
{
	unsigned long pending, srq;

	spin_lock(&rtai_lsrq_lock);
	while ((pending = rtai_sysreq_pending & ~rtai_sysreq_running)) {
		set_bit(srq = ffnz(pending), &rtai_sysreq_running);
		clear_bit(srq, &rtai_sysreq_pending);
		spin_unlock(&rtai_lsrq_lock);

		if (test_bit(srq, &rtai_sysreq_map)) {
			rtai_sysreq_table[srq].k_handler();
		}

		clear_bit(srq, &rtai_sysreq_running);
		spin_lock(&rtai_lsrq_lock);
	}
	spin_unlock(&rtai_lsrq_lock);
}


/*
 * rtai_usrq_dispatcher
 */

static inline long long rtai_usrq_dispatcher (unsigned long srq, unsigned long label)
{
	TRACE_RTAI_SRQ_ENTRY(srq);

	if (srq > 0 && srq < RTAI_NR_SRQS && test_bit(srq, &rtai_sysreq_map) && rtai_sysreq_table[srq].u_handler) {
		return rtai_sysreq_table[srq].u_handler(label);
	} else {
		for (srq = 1; srq < RTAI_NR_SRQS; srq++) {
			if (test_bit(srq, &rtai_sysreq_map) && rtai_sysreq_table[srq].label == label) {
				return (long long)srq;
			}
		}
	}

	TRACE_RTAI_SRQ_EXIT();

	return 0LL;
}


/*
 * rtai_syscall_dispatcher
 * In PowerPC registers have the following meanings:
 * gpr[0] syscall number
 * gpr[1] *
 * gpr[2] *
 * gpr[3] first syscall argument
 * gpr[4] second syscall argument
 * gpr[5] third syscall argument
 * gpr[6] ...
*/


#include <asm/rtai_usi.h>
long long (*rtai_lxrt_dispatcher)(unsigned long, unsigned long, void *);

static int (*sched_intercept_syscall_prologue)(struct pt_regs *);

static int intercept_syscall_prologue(unsigned long event, struct pt_regs *regs){
	if (likely(regs->gpr[0] >= RTAI_SYSCALL_NR)) {
		unsigned long srq  = regs->gpr[3];
		IF_IS_A_USI_SRQ_CALL_IT(srq, regs->gpr[4], (long long *)regs->gpr[5], regs->msr, 1);
		*((long long *)regs->gpr[5]) = srq > RTAI_NR_SRQS ?  rtai_lxrt_dispatcher(srq, regs->gpr[4], regs) : rtai_usrq_dispatcher(srq, regs->gpr[4]);
		if (!in_hrt_mode(srq = rtai_cpuid())) {
			hal_test_and_fast_flush_pipeline(srq);
			return 0;
		}
		return 1;
	}
	return likely(sched_intercept_syscall_prologue != NULL) ? sched_intercept_syscall_prologue(regs) : 0;
}


asmlinkage int rtai_syscall_dispatcher (struct pt_regs *regs)
{
	unsigned long srq = regs->gpr[0];

	IF_IS_A_USI_SRQ_CALL_IT(srq, regs->gpr[4], (long long *)regs->gpr[5], regs->msr, 1);

        *((long long *)regs->gpr[3]) = srq > RTAI_NR_SRQS ? rtai_lxrt_dispatcher(srq, regs->gpr[4], regs) : rtai_usrq_dispatcher(srq, regs->gpr[4]);

        if (!in_hrt_mode(srq = rtai_cpuid())) {
                hal_test_and_fast_flush_pipeline(srq);
                return 1;
        }
        return 0;
}

/*
 * rtai_install_archdep
 */

static void rtai_install_archdep (void)
{
	struct hal_sysinfo_struct sysinfo;

#if !defined(USE_LINUX_SYSCALL) && !defined(CONFIG_RTAI_LXRT_USE_LINUX_SYSCALL)
	/* empty till a direct RTAI syscall way is decided */
#endif

	hal_catch_event(hal_root_domain, HAL_SYSCALL_PROLOGUE, (void *)intercept_syscall_prologue);

	hal_get_sysinfo(&sysinfo);

	if (sysinfo.archdep.tmirq != RTAI_TIMER_DECR_IRQ) {
		printk("RTAI/ipipe: the timer interrupt %d is not supported\n", sysinfo.archdep.tmirq);
	}

	if (rtai_cpufreq_arg == 0) {
		rtai_cpufreq_arg = (unsigned long)sysinfo.cpufreq;
	}
	rtai_tunables.cpu_freq = rtai_cpufreq_arg;
}


/*
 * rtai_uninstall_archdep
 */

static void rtai_uninstall_archdep (void)
{
/* something to be added when a direct RTAI syscall way is decided */
	hal_catch_event(hal_root_domain, HAL_SYSCALL_PROLOGUE, NULL);
}


/*
 * rt_set_ihook
 */

void (*rt_set_ihook (void (*hookfn)(int)))(int)
{
#ifdef CONFIG_RTAI_SCHED_ISR_LOCK
	return (void (*)(int))xchg(&rtai_isr_hook, hookfn); /* This is atomic */
#else  /* !CONFIG_RTAI_SCHED_ISR_LOCK */
	return NULL;
#endif /* CONFIG_RTAI_SCHED_ISR_LOCK */
}


/*
 * rtai_set_linux_task_priority
 */

void rtai_set_linux_task_priority (struct task_struct *task, int policy, int prio)
{
	hal_set_linux_task_priority(task, policy, prio);
	if (task->rt_priority != prio || task->policy != policy) {
		printk("RTAI[hal]: sched_setscheduler(policy = %d, prio = %d) failed, (%s -- pid = %d)\n", policy, prio, task->comm, task->pid);
	}
}

#ifdef CONFIG_PROC_FS

struct proc_dir_entry *rtai_proc_root = NULL;


/*
 * rtai_read_proc
 */

static int rtai_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{
	PROC_PRINT_VARS;
	int i, none;

	PROC_PRINT("\n** RTAI/ppc over ADEOS/ipipe:\n\n");
	PROC_PRINT("    Decr. Frequency: %lu\n", rtai_tunables.cpu_freq);
	PROC_PRINT("    Decr. Latency: %d ns\n", RTAI_LATENCY_8254);
	PROC_PRINT("    Decr. Setup Time: %d ns\n", RTAI_SETUP_TIME_8254);

	none = 1;
	PROC_PRINT("\n** Real-time IRQs used by RTAI: ");
    	for (i = 0; i < RTAI_NR_IRQS; i++) {
		if (rtai_realtime_irq[i].handler) {
			if (none) {
				PROC_PRINT("\n");
				none = 0;
			}
			PROC_PRINT("\n    #%d at %p", i, rtai_realtime_irq[i].handler);
		}
        }
	if (none) {
		PROC_PRINT("none");
	}
	PROC_PRINT("\n\n");

	PROC_PRINT("** RTAI extension traps: \n\n");
	PROC_PRINT("    SYSREQ=0x%x\n", 0xC00);

	PROC_PRINT("    IRQ spurious = %d\n", spurious_interrupts);
	PROC_PRINT("\n");

	none = 1;
	PROC_PRINT("** RTAI SYSREQs in use: \n");
    	for (i = 0; i < RTAI_NR_SRQS; i++) {
		if (rtai_sysreq_table[i].k_handler || rtai_sysreq_table[i].u_handler || rtai_sysreq_table[i].label) {
			PROC_PRINT("    #%d label:%lu\n", i, rtai_sysreq_table[i].label);
			none = 0;
		}
        }

	if (none) {
		PROC_PRINT("    none");
	}
    	PROC_PRINT("\n\n");

	PROC_PRINT_DONE;
}


/*
 * rtai_proc_register
 */

static int rtai_proc_register (void)
{
	struct proc_dir_entry *ent;

	rtai_proc_root = create_proc_entry("rtai", S_IFDIR, 0);
	if (!rtai_proc_root) {
		printk(KERN_ERR "Unable to initialize /proc/rtai.\n");
		return -1;
        }
	rtai_proc_root->owner = THIS_MODULE;
	ent = create_proc_entry("hal", S_IFREG|S_IRUGO|S_IWUSR, rtai_proc_root);
	if (!ent) {
		printk(KERN_ERR "Unable to initialize /proc/rtai/hal.\n");
		return -1;
        }

	ent->read_proc = rtai_read_proc;

	return 0;
}


/*
 * rtai_proc_unregister
 */

static void rtai_proc_unregister (void)
{
	remove_proc_entry("hal", rtai_proc_root);
	remove_proc_entry("rtai", 0);
}

#endif /* CONFIG_PROC_FS */


/*
 * rtai_domain_entry
 */

static void rtai_domain_entry (int iflag)
{
	if (iflag) {
		rt_printk(KERN_INFO "RTAI[hal]: <%s> mounted over %s %s.\n", PACKAGE_VERSION, HAL_TYPE, HAL_VERSION_STRING);
		rt_printk(KERN_INFO "RTAI[hal]: compiled with %s.\n", CONFIG_RTAI_COMPILER);
	}
	for (;;) hal_suspend_domain();
}


/*
 * rtai_catch_event
 */

long rtai_catch_event (struct hal_domain_struct *from, unsigned long event, int (*handler)(unsigned long, void *)) {
	if (event == HAL_SYSCALL_PROLOGUE) {
		sched_intercept_syscall_prologue = (void *)handler;
		return 0;
	}
	return (long)hal_catch_event(from, event, (void *)handler);
}


/*
 * __rtai_hal_init, Execution Domain: Linux
 */

extern int ipipe_events_diverted;

int __rtai_hal_init (void)
{
	int trapnr, halinv;
	struct hal_attr_struct attr;

	// check event handler registration, check for any already installed
	for (halinv = trapnr = 0; trapnr < HAL_NR_EVENTS; trapnr++) {
		if (hal_root_domain->hal_event_handler_fun(trapnr)) {
			halinv = 1;
			printk("EVENT %d INVALID\n", trapnr);
		}
	}
	if (halinv) {
		printk(KERN_ERR "RTAI[hal]: HAL IMMEDIATE EVENT DISPATCHING BROKEN\n");
		return -1;
	}

	// request a virtual interrupt for RTAI sysrqs
	if (!(rtai_sysreq_virq = hal_alloc_irq())) {
		printk(KERN_ERR "RTAI[hal]: no virtual interrupt available.\n");
		return -1;
	}

	// copy HAL proper pointers locally for a more effective use
	for (trapnr = 0; trapnr < RTAI_NR_IRQS; trapnr++) {
		rtai_realtime_irq[trapnr].irq_ack = (void *)hal_root_domain->irqs[trapnr].acknowledge;
	}
#if LINUX_VERSION_CODE < RTAI_LT_KERNEL_VERSION_FOR_NONPERCPU
	for (trapnr = 0; trapnr < RTAI_NR_CPUS; trapnr++) {
		ipipe_root_status[trapnr] = &hal_root_domain->cpudata[trapnr].status;
	}
#endif

	// assign the RTAI sysrqs handler
	hal_virtualize_irq(hal_root_domain, rtai_sysreq_virq, &rtai_lsrq_dispatcher, NULL, IPIPE_HANDLE_MASK);

	// save the old the irq dispatcher and set rtai dispatcher ext intr
	rtai_set_gate_vector(INTR_VECTOR, rtai_hirq_dispatcher, 0);

	// architecture dependent RTAI installation
	ipipe_events_diverted = 1;
	rtai_install_archdep();

#ifdef CONFIG_PROC_FS
	rtai_proc_register();
#endif /* CONFIG_PROC_FS */

	// register RTAI domain
	hal_init_attr(&attr);
	attr.name     = "RTAI";
	attr.domid    = RTAI_DOMAIN_ID;
	attr.entry    = (void *)rtai_domain_entry;
	attr.priority = get_domain_pointer(1)->priority + 100;
	hal_register_domain(&rtai_domain, &attr);

	// register trap handler for all FAULTS in the root domain
	for (trapnr = 0; trapnr < HAL_NR_FAULTS; trapnr++) {
		hal_catch_event(hal_root_domain, trapnr, (void *)rtai_trap_fault);
	}

	// log RTAI mounted
	printk(KERN_INFO "RTAI[hal]: mounted (%s, IMMEDIATE (INTERNAL IRQs %s).\n", HAL_TYPE, CONFIG_RTAI_DONT_DISPATCH_CORE_IRQS ? "VECTORED" : "DISPATCHED");

	// log PIPELINE layers
	printk("PIPELINE layers:\n");
	for (trapnr = 1; ; trapnr++) {
		struct hal_domain_struct *next_domain;
		next_domain = get_domain_pointer(trapnr);
		if ((unsigned long)next_domain < 10) break;
		printk("%p %x %s %d\n", next_domain, next_domain->domid, next_domain->name, next_domain->priority);
	}

	return 0;
}


/*
 * __rtai_hal_exit, Execution Domain: Linux
 */

void __rtai_hal_exit (void)
{
	int trapnr;
	unsigned long flags;

#ifdef CONFIG_PROC_FS
	rtai_proc_unregister();
#endif /* CONFIG_PROC_FS */

	// restore old irq handler (__ipipe_grab_irq_intr)
	rtai_save_flags_and_cli(flags);
	rtai_reset_gate_vector(INTR_VECTOR, 0, 0);
	rtai_restore_flags(flags);

	// unregister RTAI domain
	hal_unregister_domain(&rtai_domain);

	// uninstall event catchers
	for (trapnr = 0; trapnr < HAL_NR_FAULTS; trapnr++) {
		hal_catch_event(hal_root_domain, trapnr, NULL);
	}

	// uninstall virtualized irq for RTAI sysreqs and deregister it
	hal_virtualize_irq(hal_root_domain, rtai_sysreq_virq, NULL, NULL, 0);
	hal_free_irq(rtai_sysreq_virq);

	// archdep uninstall
	rtai_uninstall_archdep();
	ipipe_events_diverted = 0;

	// log RTAI unmounted
	printk(KERN_INFO "RTAI[hal]: unmounted.\n");
}

module_init(__rtai_hal_init);
module_exit(__rtai_hal_exit);


/*
 * rt_printk
 */

#define LINE_LENGTH 200

asmlinkage int rt_printk(const char *fmt, ...)
{
	char line[LINE_LENGTH];
	va_list args;
	int r;

	va_start(args, fmt);
	r = vsnprintf(line, LINE_LENGTH, fmt, args);
	va_end(args);
	printk("%s", line);

	return r;
}


/*
 * rt_sync_printk
 */

asmlinkage int rt_sync_printk(const char *fmt, ...)
{
	char line[LINE_LENGTH];
	va_list args;
	int r;

	va_start(args, fmt);
	r = vsnprintf(line, LINE_LENGTH, fmt, args);
	va_end(args);
	hal_set_printk_sync(&rtai_domain);
	printk("%s", line);
	hal_set_printk_async(&rtai_domain);

	return r;
}

/*
 * ll2a: support for decoding long long numbers in kernel space in 2.4.xx.
 */

void *ll2a (long long ll, char *s)
{
	unsigned long i, k, ul;
	char a[20];

	if (ll < 0) {
		s[0] = 1;
		ll = -ll;
	} else {
		s[0] = 0;
	}
	i = 0;
	while (ll > 0xFFFFFFFF) {
		ll = rtai_ulldiv(ll, 10, &k);
		a[++i] = k + '0';
	}
	ul = ((unsigned long *)&ll)[LOW];
	do {
		ul = (k = ul)/10;
		a[++i] = k - ul*10 + '0';
	} while (ul);
	if (s[0]) {
		k = 1;
		s[0] = '-';
	} else {
		k = 0;
	}
	a[0] = 0;
	while ((s[k++] = a[i--]));
	return s;
}


/*
 * export section
 */

EXPORT_SYMBOL(rtai_realtime_irq);

EXPORT_SYMBOL(rt_request_irq);
EXPORT_SYMBOL(rt_release_irq);
EXPORT_SYMBOL(rt_set_irq_cookie);
EXPORT_SYMBOL(rt_set_irq_retmode);
EXPORT_SYMBOL(rt_set_irq_ack);

EXPORT_SYMBOL(rt_startup_irq);
EXPORT_SYMBOL(rt_shutdown_irq);
EXPORT_SYMBOL(rt_enable_irq);
EXPORT_SYMBOL(rt_disable_irq);
EXPORT_SYMBOL(rt_mask_and_ack_irq);
EXPORT_SYMBOL(rt_unmask_irq);
EXPORT_SYMBOL(rt_ack_irq);

EXPORT_SYMBOL(rt_request_linux_irq);
EXPORT_SYMBOL(rt_free_linux_irq);
EXPORT_SYMBOL(rt_pend_linux_irq);
EXPORT_SYMBOL(usr_rt_pend_linux_irq);

EXPORT_SYMBOL(rt_request_srq);
EXPORT_SYMBOL(rt_free_srq);
EXPORT_SYMBOL(rt_pend_linux_srq);

EXPORT_SYMBOL(rt_assign_irq_to_cpu);
EXPORT_SYMBOL(rt_reset_irq_to_sym_mode);
EXPORT_SYMBOL(rt_request_apic_timers);
EXPORT_SYMBOL(rt_free_apic_timers);

EXPORT_SYMBOL(rt_request_timer);
EXPORT_SYMBOL(rt_free_timer);
EXPORT_SYMBOL(rt_request_rtc);
EXPORT_SYMBOL(rt_release_rtc);

EXPORT_SYMBOL(rt_set_trap_handler);
EXPORT_SYMBOL(rt_set_ihook);

EXPORT_SYMBOL(rtai_critical_enter);
EXPORT_SYMBOL(rtai_critical_exit);

EXPORT_SYMBOL(rtai_set_linux_task_priority);
EXPORT_SYMBOL(rtai_linux_context);
EXPORT_SYMBOL(rtai_domain);
EXPORT_SYMBOL(rtai_proc_root);
EXPORT_SYMBOL(rtai_tunables);
EXPORT_SYMBOL(rtai_cpu_lock);
EXPORT_SYMBOL(rtai_cpu_realtime);
EXPORT_SYMBOL(rt_times);
EXPORT_SYMBOL(rt_smp_times);

EXPORT_SYMBOL(rt_printk);
EXPORT_SYMBOL(rt_sync_printk);
EXPORT_SYMBOL(ll2a);

EXPORT_SYMBOL(rtai_set_gate_vector);
EXPORT_SYMBOL(rtai_reset_gate_vector);
EXPORT_SYMBOL(rtai_catch_event);

EXPORT_SYMBOL(rtai_lxrt_dispatcher);
EXPORT_SYMBOL(rt_scheduling);
#if LINUX_VERSION_CODE < RTAI_LT_KERNEL_VERSION_FOR_NONPERCPU
EXPORT_SYMBOL(ipipe_root_status);
#endif

void up_task_sw(void *, void *);
EXPORT_SYMBOL(up_task_sw);

#ifdef CONFIG_RTAI_FPU_SUPPORT
void __save_fpenv(void *fpenv);
EXPORT_SYMBOL(__save_fpenv);
void __restore_fpenv(void *fpenv);
EXPORT_SYMBOL(__restore_fpenv);
#endif

EXPORT_SYMBOL(IsolCpusMask);

#ifdef CONFIG_GENERIC_CLOCKEVENTS

#include <linux/clockchips.h>
#include <linux/ipipe_tickdev.h>

void (*rt_linux_hrt_set_mode)(enum clock_event_mode, struct ipipe_tick_device *);
int (*rt_linux_hrt_next_shot)(unsigned long, struct ipipe_tick_device *);

/*
 * _rt_linux_hrt_set_mode and _rt_linux_hrt_next_shot below should serve
 * RTAI examples only and assume that RTAI is in periodic mode always
 */

static void _rt_linux_hrt_set_mode(enum clock_event_mode mode, struct ipipe_tick_device *hrt_dev)
{
	if (mode == CLOCK_EVT_MODE_ONESHOT || mode == CLOCK_EVT_MODE_SHUTDOWN) {
		rt_times.linux_tick = 0;
	} else if (mode == CLOCK_EVT_MODE_PERIODIC) {
		rt_times.linux_tick = rtai_llimd((1000000000 + HZ/2)/HZ, TIMER_FREQ, 1000000000);
	}
}

static int _rt_linux_hrt_next_shot(unsigned long delay, struct ipipe_tick_device *hrt_dev)
{
	rt_times.linux_time = rt_times.tick_time + rtai_llimd(delay, TIMER_FREQ, 1000000000);
	return 0;
}

#ifdef __IPIPE_FEATURE_REQUEST_TICKDEV
#define  IPIPE_REQUEST_TICKDEV(a, b, c, d, e)  ipipe_request_tickdev(a, (void *)(b), (void *)(c), d, e)
#else
#define  IPIPE_REQUEST_TICKDEV(a, b, c, d, e)  ipipe_request_tickdev(a, b, c, d)
#endif

static int rtai_request_tickdev(void)
{
	int mode, cpuid;
	unsigned long timer_freq;
	for (cpuid = 0; cpuid < num_online_cpus(); cpuid++) {
		if ((void *)rt_linux_hrt_set_mode != (void *)rt_linux_hrt_next_shot) {
			mode = IPIPE_REQUEST_TICKDEV("decrementer", rt_linux_hrt_set_mode, rt_linux_hrt_next_shot, cpuid, &timer_freq);
		} else {
			mode = IPIPE_REQUEST_TICKDEV("decrementer", _rt_linux_hrt_set_mode, _rt_linux_hrt_next_shot, cpuid, &timer_freq);
		}
		if (mode == CLOCK_EVT_MODE_UNUSED || mode == CLOCK_EVT_MODE_ONESHOT) {
			rt_times.linux_tick = 0;
		} else if (mode != CLOCK_EVT_MODE_PERIODIC) {
			return mode;
		}
	}
	return 0;
}

static void rtai_release_tickdev(void)
{
	int cpuid;
	for (cpuid = 0; cpuid < num_online_cpus(); cpuid++) {
		ipipe_release_tickdev(cpuid);
	}
}

#else /* !CONFIG_GENERIC_CLOCKEVENTS */

void (*rt_linux_hrt_set_mode)(int clock_event_mode, void *);
int (*rt_linux_hrt_next_shot)(unsigned long, void *);

static int rtai_request_tickdev(void)   { return 0; }

static void rtai_release_tickdev(void)  {  return;  }

#endif /* CONFIG_GENERIC_CLOCKEVENTS */

EXPORT_SYMBOL(rt_linux_hrt_set_mode);
EXPORT_SYMBOL(rt_linux_hrt_next_shot);
