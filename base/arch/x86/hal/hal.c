/*
 *   @ingroup hal
 *   @file
 *
 *   ARTI -- RTAI-compatible Adeos-based Real-Time Interface. Based on
 *   the original RTAI layer for x86.
 *
 *   Original RTAI/x86 layer implementation: \n
 *   Copyright &copy; 2000-2015 Paolo Mantegazza, \n
 *   Copyright &copy; 2000      Steve Papacharalambous, \n
 *   Copyright &copy; 2000      Stuart Hughes, \n
 *   and others.
 *
 *   RTAI/x86 rewrite over Adeos: \n
 *   Copyright &copy 2002 Philippe Gerum.
 *   Copyright &copy 2005 Paolo Mantegazza.
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
 * @defgroup hal RTAI services functions.
 *
 * This module defines some functions that can be used by RTAI tasks, for
 * managing interrupts and communication services with Linux processes.
 *
 *@{*/


#include <linux/module.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/console.h>
#include <asm/hw_irq.h>
#include <asm/irq.h>
#include <asm/desc.h>
#include <asm/io.h>
#include <asm/mmu_context.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#ifdef CONFIG_X86_LOCAL_APIC
#include <asm/fixmap.h>
#include <asm/bitops.h>
#include <asm/mpspec.h>
#ifdef CONFIG_X86_IO_APIC
#include <asm/io_apic.h>
#endif /* CONFIG_X86_IO_APIC */
#include <asm/apic.h>
#endif /* CONFIG_X86_LOCAL_APIC */
#define __RTAI_HAL__
#include <rtai.h>
#include <asm/rtai_hal.h>
#include <asm/rtai_lxrt.h>
#ifdef CONFIG_PROC_FS
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <rtai_proc_fs.h>
#endif /* CONFIG_PROC_FS */
#include <stdarg.h>

#ifndef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

static unsigned long rtai_cpufreq_arg = RTAI_CALIBRATED_CPU_FREQ;
RTAI_MODULE_PARM(rtai_cpufreq_arg, ulong);

#define RTAI_NR_IRQS  IPIPE_NR_IRQS

#ifdef CONFIG_X86_LOCAL_APIC
static unsigned long rtai_apicfreq_arg = RTAI_CALIBRATED_APIC_FREQ;
RTAI_MODULE_PARM(rtai_apicfreq_arg, ulong);
#endif /* CONFIG_X86_LOCAL_APIC */

struct { volatile int locked, rqsted; } rt_scheduling[RTAI_NR_CPUS];

struct hal_domain_struct rtai_domain;

struct rtai_realtime_irq_s rtai_realtime_irq[RTAI_NR_IRQS];

static struct {
	unsigned long flags;
	int count;
} rtai_linux_irq[RTAI_NR_IRQS];

static struct {
	void (*k_handler)(void);
	long long (*u_handler)(unsigned long);
	unsigned long label;
} rtai_sysreq_table[RTAI_NR_SRQS];

static unsigned rtai_sysreq_virq;

static unsigned long rtai_sysreq_map = 1; /* srq 0 is reserved */

static unsigned long rtai_sysreq_pending;

static unsigned long rtai_sysreq_running;

static DEFINE_SPINLOCK(rtai_lsrq_lock);  // SPIN_LOCK_UNLOCKED

static volatile int rtai_sync_level;

static atomic_t rtai_sync_count = ATOMIC_INIT(1);

static RT_TRAP_HANDLER rtai_trap_handler;

struct rt_times rt_times;

struct rt_times rt_smp_times[RTAI_NR_CPUS];

struct rtai_switch_data rtai_linux_context[RTAI_NR_CPUS];

struct calibration_data rtai_tunables;

volatile unsigned long rtai_cpu_realtime;

struct global_lock rtai_cpu_lock[1];

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

void rtai_critical_exit (unsigned long flags)
{
	atomic_inc(&rtai_sync_count);
	hal_critical_exit(flags);
}

unsigned long IsolCpusMask = 0;
RTAI_MODULE_PARM(IsolCpusMask, ulong);

int rt_request_irq (unsigned irq, int (*handler)(unsigned irq, void *cookie), void *cookie, int retmode)
{
	int ret;
	 ret = ipipe_request_irq(&rtai_domain, irq, (void *)handler, cookie, NULL);
	if (!ret) {
		rtai_realtime_irq[irq].retmode = retmode ? 1 : 0;
		if (IsolCpusMask && irq < IPIPE_NR_XIRQS) {
			rtai_realtime_irq[irq].cpumask = rt_assign_irq_to_cpu(irq, IsolCpusMask);
		}
	}
	return ret;
}

int rt_release_irq (unsigned irq)
{
	ipipe_free_irq(&rtai_domain, irq);
	if (IsolCpusMask && irq < IPIPE_NR_XIRQS) {
		rt_assign_irq_to_cpu(irq, rtai_realtime_irq[irq].cpumask);
	}
	return 0;
}

int rt_set_irq_ack(unsigned irq, int (*irq_ack)(unsigned int, void *))
{
	if (irq >= RTAI_NR_IRQS) {
		return -EINVAL;
	}
	rtai_domain.irqs[irq].ackfn = irq_ack ? (void *)irq_ack : hal_root_domain->irqs[irq].ackfn;
	return 0;
}

void rt_set_irq_cookie (unsigned irq, void *cookie)
{
	if (irq < RTAI_NR_IRQS) {
		rtai_domain.irqs[irq].cookie = cookie;
	}
}

void rt_set_irq_retmode (unsigned irq, int retmode)
{
	if (irq < RTAI_NR_IRQS) {
		rtai_realtime_irq[irq].retmode = retmode ? 1 : 0;
	}
}


// A bunch of macros to support Linux developers moods in relation to 
// interrupt handling across various releases.
// Here we care about ProgrammableInterruptControllers (PIC) in particular.

// 1 - IRQs descriptor and chip
#define rtai_irq_desc(irq) (irq_to_desc(irq))[0]
#define rtai_irq_desc_chip(irq) (irq_to_desc(irq)->irq_data.chip)

// 2 - IRQs atomic protections
#define rtai_irq_desc_lock(irq, flags) raw_spin_lock_irqsave(&rtai_irq_desc(irq).lock, flags)
#define rtai_irq_desc_unlock(irq, flags) raw_spin_unlock_irqrestore(&rtai_irq_desc(irq).lock, flags)

// 3 - IRQs enabling/disabling naming and calling
#define rtai_irq_endis_fun(fun, irq) irq_##fun(&(rtai_irq_desc(irq).irq_data)) 

unsigned rt_startup_irq (unsigned irq)
{
	return rtai_irq_desc_chip(irq)->rtai_irq_endis_fun(startup, irq);
}

void rt_shutdown_irq (unsigned irq)
{
	rtai_irq_desc_chip(irq)->rtai_irq_endis_fun(shutdown, irq);
}

static inline void _rt_enable_irq (unsigned irq)
{
	if (rtai_irq_desc_chip(irq)->irq_enable) {
		rtai_irq_desc_chip(irq)->rtai_irq_endis_fun(enable, irq);
	} else {
		rtai_irq_desc_chip(irq)->rtai_irq_endis_fun(unmask, irq);
	}
}

void rt_enable_irq (unsigned irq)
{
	_rt_enable_irq(irq);
}

void rt_disable_irq (unsigned irq)
{
	if (rtai_irq_desc_chip(irq)->irq_disable) {
		rtai_irq_desc_chip(irq)->rtai_irq_endis_fun(disable, irq);
	} else {
		rtai_irq_desc_chip(irq)->rtai_irq_endis_fun(mask, irq);
	}
}

void rt_mask_and_ack_irq (unsigned irq)
{
	rtai_irq_desc_chip(irq)->rtai_irq_endis_fun(mask_ack, irq);
}

void rt_mask_irq (unsigned irq)
{
	rtai_irq_desc_chip(irq)->rtai_irq_endis_fun(mask, irq);
}

void rt_unmask_irq (unsigned irq)
{
	rtai_irq_desc_chip(irq)->rtai_irq_endis_fun(unmask, irq);
}

void rt_ack_irq (unsigned irq)
{
	_rt_enable_irq(irq);
}

void rt_end_irq (unsigned irq)
{
	_rt_enable_irq(irq);
}

void rt_eoi_irq (unsigned irq)
{
        rtai_irq_desc_chip(irq)->rtai_irq_endis_fun(eoi, irq);
}

int rt_request_linux_irq (unsigned irq, void *handler, char *name, void *dev_id)
{
	unsigned long flags;
	int retval;

	if (irq >= RTAI_NR_IRQS || !handler) {
		return -EINVAL;
	}

	rtai_save_flags_and_cli(flags);
	spin_lock(&rtai_irq_desc(irq).lock);
	if (rtai_linux_irq[irq].count++ == 0 && rtai_irq_desc(irq).action) {
		rtai_linux_irq[irq].flags = rtai_irq_desc(irq).action->flags;
		rtai_irq_desc(irq).action->flags |= IRQF_SHARED;
	}
	spin_unlock(&rtai_irq_desc(irq).lock);
	rtai_restore_flags(flags);

	retval = request_irq(irq, handler, IRQF_SHARED, name, dev_id);

	return retval;
}

int rt_free_linux_irq (unsigned irq, void *dev_id)
{
	unsigned long flags;

	if (irq >= RTAI_NR_IRQS || rtai_linux_irq[irq].count == 0) {
		return -EINVAL;
	}

	rtai_save_flags_and_cli(flags);
	free_irq(irq, dev_id);
	spin_lock(&rtai_irq_desc(irq).lock);
	if (--rtai_linux_irq[irq].count == 0 && rtai_irq_desc(irq).action) {
		rtai_irq_desc(irq).action->flags = rtai_linux_irq[irq].flags;
	}
	spin_unlock(&rtai_irq_desc(irq).lock);
	rtai_restore_flags(flags);

	return 0;
}

void rt_pend_linux_irq (unsigned irq)
{
	unsigned long flags;
	rtai_save_flags_and_cli(flags);
	hal_pend_uncond(irq, rtai_cpuid());
	rtai_restore_flags(flags);
}

RTAI_SYSCALL_MODE void usr_rt_pend_linux_irq (unsigned irq)
{
	unsigned long flags;
	rtai_save_flags_and_cli(flags);
	hal_pend_uncond(irq, rtai_cpuid());
	rtai_restore_flags(flags);
}

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

int rt_free_srq (unsigned srq)
{
	return  (srq < 1 || srq >= RTAI_NR_SRQS || !test_and_clear_bit(srq, &rtai_sysreq_map)) ? -EINVAL : 0;
}

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

#include <linux/ipipe_tickdev.h>
void rt_linux_hrt_set_mode(enum clock_event_mode mode, struct clock_event_device *hrt_dev)
{
	if (mode == CLOCK_EVT_MODE_ONESHOT || mode == CLOCK_EVT_MODE_SHUTDOWN) {
		rt_times.linux_tick = 0;
	} else if (mode == CLOCK_EVT_MODE_PERIODIC) {
		rt_times.linux_tick = rtai_llimd((1000000000 + HZ/2)/HZ, rtai_tunables.cpu_freq, 1000000000);
	}
}

void *rt_linux_hrt_next_shot;
EXPORT_SYMBOL(rt_linux_hrt_next_shot);

int _rt_linux_hrt_next_shot(unsigned long delay, struct clock_event_device *hrt_dev)
{
	rt_times.linux_time = rt_times.tick_time + rtai_llimd(delay, TIMER_FREQ, 1000000000);
	return 0;
}

int rt_request_timers(void *rtai_time_handler)
{
	int cpuid;

	if (!rt_linux_hrt_next_shot) {
		rt_linux_hrt_next_shot = _rt_linux_hrt_next_shot;
	}
	for (cpuid = 0; cpuid < num_active_cpus(); cpuid++) {
		struct rt_times *rtimes;
		int ret;
		ret = ipipe_timer_start(rtai_time_handler, rt_linux_hrt_set_mode, rt_linux_hrt_next_shot, cpuid);
		if (ret < 0 || ret == CLOCK_EVT_MODE_SHUTDOWN) {
			printk("THE TIMERS REQUEST FAILED RETURNING %d FOR CPUID %d, FREEING ALL TIMERS.\n", ret, cpuid);
			do {
				ipipe_timer_stop(cpuid);
			} while (--cpuid >= 0);
			return -1;
		}
		rtimes = &rt_smp_times[cpuid];
		if (ret == CLOCK_EVT_MODE_ONESHOT || ret == CLOCK_EVT_MODE_UNUSED) {
			rtimes->linux_tick = 0;
		} else {
			rt_times.linux_tick = rtai_llimd((1000000000 + HZ/2)/HZ, rtai_tunables.cpu_freq, 1000000000);
		}			
		rtimes->tick_time  = rtai_rdtsc();
                rtimes->intr_time  = rtimes->tick_time + rtimes->linux_tick;
                rtimes->linux_time = rtimes->tick_time + rtimes->linux_tick;
		rtimes->periodic_tick = rtimes->linux_tick;
	}
	rt_times = rt_smp_times[0];
#if 0 // #ifndef CONFIG_X86_LOCAL_APIC, for calibrating 8254 with our set delay
	rtai_cli();
	outb(0x30, 0x43);
	rt_set_timer_delay(rtai_tunables.cpu_freq/50000);
	rtai_sti();
#endif
	return 0;
}
EXPORT_SYMBOL(rt_request_timers);

void rt_free_timers(void)
{
	int cpuid;
	for (cpuid = 0; cpuid < num_active_cpus(); cpuid++) {
		ipipe_timer_stop(cpuid);
	}
	rt_linux_hrt_next_shot = NULL;
}
EXPORT_SYMBOL(rt_free_timers);

#ifdef CONFIG_SMP

static unsigned long rtai_old_irq_affinity[IPIPE_NR_XIRQS];
static unsigned long rtai_orig_irq_affinity[IPIPE_NR_XIRQS];

static DEFINE_SPINLOCK(rtai_iset_lock);  // SPIN_LOCK_UNLOCKED

unsigned long rt_assign_irq_to_cpu (int irq, unsigned long cpumask)
{
	if (irq >= IPIPE_NR_XIRQS || &rtai_irq_desc(irq) == NULL || rtai_irq_desc_chip(irq) == NULL || rtai_irq_desc_chip(irq)->irq_set_affinity == NULL) {
		return 0;
	} else {
		unsigned long oldmask, flags;

		rtai_save_flags_and_cli(flags);
		spin_lock(&rtai_iset_lock);
		cpumask_copy((void *)&oldmask, irq_to_desc(irq)->irq_data.affinity);
		hal_set_irq_affinity(irq, CPUMASK_T(cpumask)); 
		if (oldmask) {
			rtai_old_irq_affinity[irq] = oldmask;
		}
		spin_unlock(&rtai_iset_lock);
		rtai_restore_flags(flags);

		return oldmask;
	}
}

unsigned long rt_reset_irq_to_sym_mode (int irq)
{
	unsigned long oldmask, flags;

	if (irq >= IPIPE_NR_XIRQS) {
		return 0;
	} else {
		rtai_save_flags_and_cli(flags);
		spin_lock(&rtai_iset_lock);
		if (rtai_old_irq_affinity[irq] == 0) {
			spin_unlock(&rtai_iset_lock);
			rtai_restore_flags(flags);
			return -EINVAL;
		}
		cpumask_copy((void *)&oldmask, irq_to_desc(irq)->irq_data.affinity);
		if (rtai_old_irq_affinity[irq]) {
	        	hal_set_irq_affinity(irq, CPUMASK_T(rtai_old_irq_affinity[irq]));
	        	rtai_old_irq_affinity[irq] = 0;
        	}
		spin_unlock(&rtai_iset_lock);
		rtai_restore_flags(flags);

		return oldmask;
	}
}

#else  /* !CONFIG_SMP */

unsigned long rt_assign_irq_to_cpu (int irq, unsigned long cpus_mask)
{
	return 0;
}

unsigned long rt_reset_irq_to_sym_mode (int irq)
{
	return 0;
}

#endif /* CONFIG_SMP */

RT_TRAP_HANDLER rt_set_trap_handler (RT_TRAP_HANDLER handler)
{
	return (RT_TRAP_HANDLER)xchg(&rtai_trap_handler, handler);
}

#define HAL_LOCK_LINUX() \
do { sflags = rt_save_switch_to_real_time(cpuid = rtai_cpuid()); } while (0)
#define HAL_UNLOCK_LINUX() \
do { rtai_cli(); rt_restore_switch_to_linux(sflags, cpuid); } while (0)

#ifdef CONFIG_RTAI_SCHED_ISR_LOCK
void (*rtai_isr_sched)(int cpuid);
EXPORT_SYMBOL(rtai_isr_sched);
#define RTAI_SCHED_ISR_LOCK() \
	do { \
		if (!rt_scheduling[cpuid].locked++) { \
			rt_scheduling[cpuid].rqsted = 0; \
		} \
	} while (0)
#define RTAI_SCHED_ISR_UNLOCK() \
	do { \
		if (rt_scheduling[cpuid].locked && !(--rt_scheduling[cpuid].locked)) { \
			if (rt_scheduling[cpuid].rqsted > 0 && rtai_isr_sched) { \
				rtai_isr_sched(cpuid); \
        		} \
		} \
	} while (0)
#else  /* !CONFIG_RTAI_SCHED_ISR_LOCK */
#define RTAI_SCHED_ISR_LOCK() \
	do {                       } while (0)
//	do { cpuid = rtai_cpuid(); } while (0)
#define RTAI_SCHED_ISR_UNLOCK() \
	do {                       } while (0)
#endif /* CONFIG_RTAI_SCHED_ISR_LOCK */

static int rtai_hirq_dispatcher(int irq)
{
	unsigned long cpuid;
	if (rtai_domain.irqs[irq].handler) {
		unsigned long sflags;
		HAL_LOCK_LINUX();
		RTAI_SCHED_ISR_LOCK();
		rtai_domain.irqs[irq].handler(irq, rtai_domain.irqs[irq].cookie);
		RTAI_SCHED_ISR_UNLOCK();
		HAL_UNLOCK_LINUX();
		if (rtai_realtime_irq[irq].retmode || test_bit(IPIPE_STALL_FLAG, ROOT_STATUS_ADR(cpuid))) {
			return 0;
		}
	}
	rtai_sti();
	hal_fast_flush_pipeline(cpuid);
	return 0;
}

//#define HINT_DIAG_ECHO
//#define HINT_DIAG_TRAPS

#ifdef HINT_DIAG_ECHO
#define HINT_DIAG_MSG(x) x
#else
#define HINT_DIAG_MSG(x)
#endif

static int PrintFpuTrap = 0;
RTAI_MODULE_PARM(PrintFpuTrap, int);
static int PrintFpuInit = 0;
RTAI_MODULE_PARM(PrintFpuInit, int);

static int rtai_trap_fault (unsigned trap, struct pt_regs *regs)
{
#ifdef HINT_DIAG_TRAPS
	static unsigned long traps_in_hard_intr = 0;
        do {
                unsigned long flags;
                rtai_save_flags_and_cli(flags);
                if (!test_bit(RTAI_IFLAG, &flags)) {
                        if (!test_and_set_bit(trap, &traps_in_hard_intr)) {
                                HINT_DIAG_MSG(rt_printk("TRAP %d HAS INTERRUPT DISABLED (TRAPS PICTURE %lx).\n", trap, traps_in_hard_intr););
                        }
                }
        } while (0);
#endif
	static const int trap2sig[] = {
    		SIGFPE,         //  0 - Divide error
		SIGTRAP,        //  1 - Debug
		SIGSEGV,        //  2 - NMI (but we ignore these)
		SIGTRAP,        //  3 - Software breakpoint
		SIGSEGV,        //  4 - Overflow
		SIGSEGV,        //  5 - Bounds
		SIGILL,         //  6 - Invalid opcode
		SIGSEGV,        //  7 - Device not available
		SIGSEGV,        //  8 - Double fault
		SIGFPE,         //  9 - Coprocessor segment overrun
		SIGSEGV,        // 10 - Invalid TSS
		SIGBUS,         // 11 - Segment not present
		SIGBUS,         // 12 - Stack segment
		SIGSEGV,        // 13 - General protection fault
		SIGSEGV,        // 14 - Page fault
		0,              // 15 - Spurious interrupt
		SIGFPE,         // 16 - Coprocessor error
		SIGBUS,         // 17 - Alignment check
		SIGSEGV,        // 18 - Reserved
		SIGFPE,         // 19 - XMM fault
		0,0,0,0,0,0,0,0,0,0,0,0
	};
	if (!in_hrt_mode(rtai_cpuid())) {
		goto propagate;
	}
	if (trap == 7)	{
		struct task_struct *linux_task = current;
		rtai_cli();
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
		rtai_sti();
		return 1;
	}
	if (rtai_trap_handler && rtai_trap_handler(trap, trap2sig[trap], regs, NULL)) {
		return 1;
	}
propagate:
	return 0;
}

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

long long rtai_usrq_dispatcher (unsigned long srq, unsigned long label)
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
EXPORT_SYMBOL(rtai_usrq_dispatcher);

#include <asm/rtai_usi.h>

static int hal_intercept_syscall(struct pt_regs *regs)
{
	if (likely(regs->LINUX_SYSCALL_NR >= RTAI_SYSCALL_NR)) {
		unsigned long srq = regs->LINUX_SYSCALL_REG1;
		IF_IS_A_USI_SRQ_CALL_IT(srq, regs->LINUX_SYSCALL_REG2, (long long *)regs->LINUX_SYSCALL_REG3, regs->LINUX_SYSCALL_FLAGS, 1);
		*((long long *)regs->LINUX_SYSCALL_REG3) = rtai_usrq_dispatcher(srq, regs->LINUX_SYSCALL_REG2);
		hal_test_and_fast_flush_pipeline(rtai_cpuid());
		return 1;
	}
	return 0;
}

void rtai_uvec_handler(void);

#include <linux/clockchips.h>
#include <linux/ipipe_tickdev.h>

extern int (*rtai_syscall_hook)(struct pt_regs *);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
static int errno;

static inline _syscall3(int, sched_setscheduler, pid_t,pid, int,policy, struct sched_param *,param)
#endif

void rtai_set_linux_task_priority (struct task_struct *task, int policy, int prio)
{
	struct sched_param param = { .sched_priority = prio };
	sched_setscheduler(task, policy, &param);
	if (task->rt_priority != prio || task->policy != policy) {
		printk("RTAI[hal]: sched_setscheduler(policy = %d, prio = %d) failed, (%s -- pid = %d)\n", policy, prio, task->comm, task->pid);
	}
}

#ifdef CONFIG_PROC_FS

struct proc_dir_entry *rtai_proc_root = NULL;

#if defined(CONFIG_SMP) && defined(CONFIG_RTAI_DIAG_TSC_SYNC)
extern void init_tsc_sync(void);
extern void cleanup_tsc_sync(void);
extern volatile long rtai_tsc_ofst[];
#endif

static int PROC_READ_FUN(rtai_read_proc)
{
	int i, none;
	PROC_PRINT_VARS;

	PROC_PRINT("\n** RTAI/x86:\n\n");
	PROC_PRINT("    CPU   Frequency: %lu (Hz)\n", rtai_tunables.cpu_freq);
	PROC_PRINT("    TIMER Frequency: %lu (Hz)\n", TIMER_FREQ);
	PROC_PRINT("    TIMER Latency: %d (ns)\n", rtai_imuldiv(rtai_tunables.latency, 1000000000, rtai_tunables.cpu_freq));
	PROC_PRINT("    TIMER Setup: %d (ns)\n", rtai_imuldiv(rtai_tunables.setup_time_TIMER_CPUNIT, 1000000000, rtai_tunables.cpu_freq));
    
	none = 1;
	PROC_PRINT("\n** Real-time IRQs used by RTAI: ");
    	for (i = 0; i < RTAI_NR_IRQS; i++) {
		if (rtai_domain.irqs[i].handler) {
			if (none) {
				PROC_PRINT("\n");
				none = 0;
			}
			PROC_PRINT("\n    #%d at %p", i, rtai_domain.irqs[i].handler);
		}
        }
	if (none) {
		PROC_PRINT("none");
	}
	PROC_PRINT("\n\n");

	PROC_PRINT("** RTAI extension traps: \n\n");

	none = 1;
	PROC_PRINT("** RTAI SYSREQs in use: ");
    	for (i = 0; i < RTAI_NR_SRQS; i++) {
		if (rtai_sysreq_table[i].k_handler || rtai_sysreq_table[i].u_handler) {
			PROC_PRINT("#%d ", i);
			none = 0;
		}
        }
	if (none) {
		PROC_PRINT("none");
	}
    	PROC_PRINT("\n\n");

#ifdef CONFIG_SMP
#ifdef CONFIG_RTAI_DIAG_TSC_SYNC
	PROC_PRINT("** RTAI TSC OFFSETs (TSC units, 0 ref. CPU): ");
    	for (i = 0; i < num_online_cpus(); i++) {
		PROC_PRINT("CPU#%d: %ld; ", i, rtai_tsc_ofst[i]);
        }
    	PROC_PRINT("\n\n");
#endif
	PROC_PRINT("** MASK OF CPUs ISOLATED FOR RTAI: 0x%lx.", IsolCpusMask);
    	PROC_PRINT("\n\n");
#endif

	PROC_PRINT_DONE;
}

PROC_READ_OPEN_OPS(rtai_hal_proc_fops, rtai_read_proc);

static int rtai_proc_register (void)
{
	struct proc_dir_entry *ent;

	rtai_proc_root = CREATE_PROC_ENTRY("rtai", S_IFDIR, NULL, &rtai_hal_proc_fops);
	if (!rtai_proc_root) {
		printk(KERN_ERR "Unable to initialize /proc/rtai.\n");
		return -1;
        }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
	rtai_proc_root->owner = THIS_MODULE;
#endif
	ent = CREATE_PROC_ENTRY("hal", S_IFREG|S_IRUGO|S_IWUSR, rtai_proc_root,
&rtai_hal_proc_fops);
	if (!ent) {
		printk(KERN_ERR "Unable to initialize /proc/rtai/hal.\n");
		return -1;
        }
	SET_PROC_READ_ENTRY(ent, rtai_read_proc);

	return 0;
}

static void rtai_proc_unregister (void)
{
	remove_proc_entry("hal", rtai_proc_root);
	remove_proc_entry("rtai", 0);
}

#endif /* CONFIG_PROC_FS */

extern struct ipipe_domain ipipe_root;
extern unsigned long cpu_isolated_map; 
extern void (*dispatch_irq_head)(unsigned int);
extern int (*rtai_trap_hook)(unsigned, struct pt_regs *);

int __rtai_hal_init (void)
{
	int i, ret = 0;
	struct hal_sysinfo_struct sysinfo;

	if (num_online_cpus() > RTAI_NR_CPUS) {
		printk("RTAI[hal]: RTAI CONFIGURED WITH LESS THAN NUM ONLINE CPUS.\n");
		ret = 1;
	}

#ifndef CONFIG_X86_TSC
	printk("RTAI[hal]: TIME STAMP CLOCK (TSC) NEEDED FOR THIS RTAI VERSION\n.");
	ret = 1;
#endif

#ifdef CONFIG_X86_LOCAL_APIC
	if (!boot_cpu_has(X86_FEATURE_APIC)) {
		printk("RTAI[hal]: ERROR, LOCAL APIC CONFIGURED BUT NOT AVAILABLE/ENABLED.\n");
		ret = 1;
	}
#endif

	if (!(rtai_sysreq_virq = ipipe_alloc_virq())) {
		printk(KERN_ERR "RTAI[hal]: NO VIRTUAL INTERRUPT AVAILABLE.\n");
		ret = 1;
	}

	if (ret) {
		return -1;
	}

	for (i = 0; i < RTAI_NR_IRQS; i++) {
		rtai_domain.irqs[i].ackfn = (void *)hal_root_domain->irqs[i].ackfn;
	}

	ipipe_request_irq(hal_root_domain, rtai_sysreq_virq, (void *)rtai_lsrq_dispatcher, NULL, NULL);
	dispatch_irq_head = (void *)rtai_hirq_dispatcher;

	ipipe_select_timers(cpu_active_mask);
	rtai_syscall_hook = hal_intercept_syscall;
	hal_get_sysinfo(&sysinfo);

	if (rtai_cpufreq_arg == 0) {
		rtai_cpufreq_arg = (unsigned long)sysinfo.sys_cpu_freq;
	}
	rtai_tunables.cpu_freq = rtai_cpufreq_arg;

#ifdef CONFIG_X86_LOCAL_APIC
	if (rtai_apicfreq_arg == 0) {
		rtai_apicfreq_arg = sysinfo.sys_hrtimer_freq;
	}
	rtai_tunables.apic_freq = rtai_apicfreq_arg;
#endif

#ifdef CONFIG_PROC_FS
	rtai_proc_register();
#endif

	ipipe_register_head(&rtai_domain, "RTAI");
	rtai_trap_hook = rtai_trap_fault;

#ifdef CONFIG_SMP
	if (IsolCpusMask && (IsolCpusMask != cpu_isolated_map)) {
		printk("\nWARNING: IsolCpusMask (%lu) does not match cpu_isolated_map (%lu) set at boot time.\n", IsolCpusMask, cpu_isolated_map);
	}
	if (!IsolCpusMask) {
		IsolCpusMask = cpu_isolated_map;
	}
	if (IsolCpusMask) {
		for (i = 0; i < IPIPE_NR_XIRQS; i++) {
			rtai_orig_irq_affinity[i] = rt_assign_irq_to_cpu(i, ~IsolCpusMask);
		}
	}
#else
	IsolCpusMask = 0;
#endif

	printk(KERN_INFO "RTAI[hal]: mounted (%s, IMMEDIATE (INTERNAL IRQs %s), ISOL_CPUS_MASK: %lx).\n", HAL_TYPE, CONFIG_RTAI_DONT_DISPATCH_CORE_IRQS ? "VECTORED" : "DISPATCHED", IsolCpusMask);

#if defined(CONFIG_SMP) && defined(CONFIG_RTAI_DIAG_TSC_SYNC)
	init_tsc_sync();
#endif

	printk("SYSINFO - # CPUs: %d, TIMER NAME: '%s', TIMER IRQ: %d, TIMER FREQ: %llu, CLOCK NAME: '%s', CLOCK FREQ: %llu, CPU FREQ: %llu.\n", sysinfo.sys_nr_cpus, ipipe_timer_name(), sysinfo.sys_hrtimer_irq, sysinfo.sys_hrtimer_freq, ipipe_clock_name(), sysinfo.sys_hrclock_freq, sysinfo.sys_cpu_freq); 

	return 0;
}

void __rtai_hal_exit (void)
{
	int i;
#ifdef CONFIG_PROC_FS
	rtai_proc_unregister();
#endif
	ipipe_unregister_head(&rtai_domain);
	dispatch_irq_head = NULL;
	rtai_trap_hook = NULL;
	ipipe_free_irq(hal_root_domain, rtai_sysreq_virq);
	ipipe_free_virq(rtai_sysreq_virq);

	ipipe_timers_release();
	rtai_syscall_hook = NULL;

	if (IsolCpusMask) {
		for (i = 0; i < IPIPE_NR_XIRQS; i++) {
			rt_reset_irq_to_sym_mode(i);
		}
	}

#if defined(CONFIG_SMP) && defined(CONFIG_RTAI_DIAG_TSC_SYNC)
	cleanup_tsc_sync();
#endif

	printk(KERN_INFO "RTAI[hal]: unmounted.\n");
}

module_init(__rtai_hal_init);
module_exit(__rtai_hal_exit);

#define VSNPRINTF_BUF 256
asmlinkage int rt_printk(const char *fmt, ...)
{
	char buf[VSNPRINTF_BUF];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, VSNPRINTF_BUF, fmt, args);
	va_end(args);
	return printk("%s", buf);
}

asmlinkage int rt_sync_printk(const char *fmt, ...)
{
	char buf[VSNPRINTF_BUF];
	va_list args;

        va_start(args, fmt);
        vsnprintf(buf, VSNPRINTF_BUF, fmt, args);
        va_end(args);
	ipipe_prepare_panic();
	return printk("%s", buf);
}

extern struct calibration_data rtai_tunables;
#define CAL_LOOPS 200
int rtai_calibrate_hard_timer(void)
{
        unsigned long flags;
        RTIME t;
	int i, delay, dt;

	delay = rtai_tunables.cpu_freq/50000;
        flags = rtai_critical_enter(NULL);
	rt_set_timer_delay(delay);
        t = rtai_rdtsc();
        for (i = 0; i < CAL_LOOPS; i++) {
		rt_set_timer_delay(delay);
        }
	dt = (int)(rtai_rdtsc() - t);
	rtai_critical_exit(flags);
	return rtai_imuldiv((dt + CAL_LOOPS/2)/CAL_LOOPS, 1000000000, rtai_tunables.cpu_freq);
}

EXPORT_SYMBOL(rtai_calibrate_hard_timer);
EXPORT_SYMBOL(rtai_realtime_irq);
EXPORT_SYMBOL(rt_request_irq);
EXPORT_SYMBOL(rt_release_irq);
EXPORT_SYMBOL(rt_set_irq_cookie);
EXPORT_SYMBOL(rt_set_irq_retmode);
EXPORT_SYMBOL(rt_startup_irq);
EXPORT_SYMBOL(rt_shutdown_irq);
EXPORT_SYMBOL(rt_enable_irq);
EXPORT_SYMBOL(rt_disable_irq);
EXPORT_SYMBOL(rt_mask_and_ack_irq);
EXPORT_SYMBOL(rt_mask_irq);
EXPORT_SYMBOL(rt_unmask_irq);
EXPORT_SYMBOL(rt_ack_irq);
EXPORT_SYMBOL(rt_end_irq);
EXPORT_SYMBOL(rt_eoi_irq);
EXPORT_SYMBOL(rt_request_linux_irq);
EXPORT_SYMBOL(rt_free_linux_irq);
EXPORT_SYMBOL(rt_pend_linux_irq);
EXPORT_SYMBOL(usr_rt_pend_linux_irq);
EXPORT_SYMBOL(rt_request_srq);
EXPORT_SYMBOL(rt_free_srq);
EXPORT_SYMBOL(rt_pend_linux_srq);
EXPORT_SYMBOL(rt_assign_irq_to_cpu);
EXPORT_SYMBOL(rt_reset_irq_to_sym_mode);
EXPORT_SYMBOL(rt_set_trap_handler);
EXPORT_SYMBOL(rt_set_irq_ack);

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

EXPORT_SYMBOL(rt_scheduling);

EXPORT_SYMBOL(IsolCpusMask);

#ifdef __i386__

#if defined(CONFIG_SMP) && defined(CONFIG_RTAI_DIAG_TSC_SYNC)

/*
	Hacked from arch/ia64/kernel/smpboot.c.
*/

//#define DIAG_OUT_OF_SYNC_TSC

#ifdef DIAG_OUT_OF_SYNC_TSC
static int sync_cnt[RTAI_NR_CPUS];
#endif

volatile long rtai_tsc_ofst[RTAI_NR_CPUS];
EXPORT_SYMBOL(rtai_tsc_ofst);

static inline long long readtsc(void)
{
	long long t;
	__asm__ __volatile__("rdtsc" : "=A" (t));
	return t;
}

#define MASTER	(0)
#define SLAVE	(SMP_CACHE_BYTES/8)

#define NUM_ITERS  10

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static spinlock_t tsc_sync_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t tsclock = SPIN_LOCK_UNLOCKED;
#else
static DEFINE_SPINLOCK(tsc_sync_lock);
static DEFINE_SPINLOCK(tsclock);
#endif

static volatile long long go[SLAVE + 1];

static void sync_master(void *arg)
{
	unsigned long flags, lflags, i;

	if ((unsigned long)arg != hal_processor_id()) {
		return;
	}

	go[MASTER] = 0;
	local_irq_save(flags);
	for (i = 0; i < NUM_ITERS; ++i) {
		while (!go[MASTER]) {
			cpu_relax();
		}
		go[MASTER] = 0;
		spin_lock_irqsave(&tsclock, lflags);
		go[SLAVE] = readtsc();
		spin_unlock_irqrestore(&tsclock, lflags);
	}
	local_irq_restore(flags);
}

static int first_sync_loop_done;
static unsigned long worst_tsc_round_trip[RTAI_NR_CPUS];

static inline long long get_delta(long long *rt, long long *master, unsigned int slave)
{
	unsigned long long best_t0 = 0, best_t1 = ~0ULL, best_tm = 0;
	unsigned long long tcenter = 0, t0, t1, tm, dt;
	unsigned long lflags;
	long i, done;

	for (done = i = 0; i < NUM_ITERS; ++i) {
		t0 = readtsc();
		go[MASTER] = 1;
		spin_lock_irqsave(&tsclock, lflags);
		while (!(tm = go[SLAVE])) {
			spin_unlock_irqrestore(&tsclock, lflags);
			cpu_relax();
			spin_lock_irqsave(&tsclock, lflags);
		}
		spin_unlock_irqrestore(&tsclock, lflags);
		go[SLAVE] = 0;
		t1 = readtsc();
		dt = t1 - t0;
		if (!first_sync_loop_done && dt > worst_tsc_round_trip[slave]) {
			worst_tsc_round_trip[slave] = dt;
		}
		if (dt < (best_t1 - best_t0) && (dt <= worst_tsc_round_trip[slave] || !first_sync_loop_done)) {
			done = 1;
			best_t0 = t0, best_t1 = t1, best_tm = tm;
		}
	}

	if (done) {
		*rt = best_t1 - best_t0;
		*master = best_tm - best_t0;
		tcenter = best_t0/2 + best_t1/2;
		if (best_t0 % 2 + best_t1 % 2 == 2) {
			++tcenter;
		}
	}
	if (!first_sync_loop_done) {
		worst_tsc_round_trip[slave] = (worst_tsc_round_trip[slave]*120)/100;
		first_sync_loop_done = 1;
		return done ? rtai_tsc_ofst[slave] = tcenter - best_tm : 0;
	}
	return done ? rtai_tsc_ofst[slave] = (8*rtai_tsc_ofst[slave] + 2*((long)(tcenter - best_tm)))/10 : 0;
}

static void sync_tsc(unsigned long master, unsigned int slave)
{
	unsigned long flags;
	long long delta, rt = 0, master_time_stamp = 0;

	go[MASTER] = 1;
	if (smp_call_function(sync_master, (void *)master,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
 							   1,
#endif
							      0) < 0) {
//		printk(KERN_ERR "sync_tsc: slave CPU %u failed to get attention from master CPU %u!\n", slave, master);
		return;
	}
	while (go[MASTER]) {
		cpu_relax();	/* wait for master to be ready */
	}
	spin_lock_irqsave(&tsc_sync_lock, flags);
	delta = get_delta(&rt, &master_time_stamp, slave);
	spin_unlock_irqrestore(&tsc_sync_lock, flags);

#ifdef DIAG_OUT_OF_SYNC_TSC
	printk(KERN_INFO "# %d - CPU %u: synced its TSC with CPU %u (master time stamp %llu cycles, < - OFFSET %lld cycles - > , max double tsc read span %llu cycles)\n", ++sync_cnt[slave], slave, master, master_time_stamp, delta, rt);
#endif
}

//#define CONFIG_RTAI_MASTER_TSC_CPU  0
#define SLEEP0  500 // ms
#define DSLEEP  500 // ms
static volatile int end;

static void kthread_fun(void *null)
{
	int i;
	while (!end) {
		for (i = 0; i < num_online_cpus(); i++) {
			if (i != CONFIG_RTAI_MASTER_TSC_CPU) {
				set_cpus_allowed(current, cpumask_of_cpu(i));
				sync_tsc(CONFIG_RTAI_MASTER_TSC_CPU, i);
			}
		}
		msleep(SLEEP0 + irandu(DSLEEP));
	}
	end = 0;
}

#include <linux/kthread.h>

void init_tsc_sync(void)
{
	if (num_online_cpus() > 1) {
		kthread_run((void *)kthread_fun, NULL, "RTAI_TSC_SYNC");
		while(!first_sync_loop_done) {
			msleep(100);
		}
	}
}

void cleanup_tsc_sync(void)
{
	if (num_online_cpus() > 1) {
		end = 1;
		while(end) {
			msleep(100);
		}
	}
}

#endif /* defined(CONFIG_SMP) && defined(CONFIG_RTAI_DIAG_TSC_SYNC) */

#endif

