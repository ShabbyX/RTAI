/**
 *   @ingroup hal
 *   @file
 *
 *   ARTI -- RTAI-compatible Adeos-based Real-Time Interface. Based on
 *   the original RTAI layer for x86.
 *
 *   Original RTAI/x86 layer implementation:
 *   Copyright &copy; 2000 Paolo Mantegazza,
 *   Copyright &copy; 2000 Steve Papacharalambous,
 *   Copyright &copy; 2000 Stuart Hughes,
 *   and others.
 *
 *   RTAI/x86 rewrite over Adeos:
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

#include <asm/rtai_hal.h>

#undef INCLUDED_BY_HAL_C
#define INCLUDED_BY_HAL_C

#define CHECK_STACK_IN_IRQ  0

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/interrupt.h>
//#include <linux/irq.h>
#include <linux/console.h>
#include <asm/system.h>
//#include <asm/hw_irq.h>
//#include <asm/irq.h>
#include <asm/machdep.h>
#include <asm/io.h>
#include <asm/mmu_context.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <asm/mcfsim.h>
#define __RTAI_HAL__
#include <asm/rtai_hal.h>
#include <asm/rtai_lxrt.h>
#ifdef CONFIG_PROC_FS
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <rtai_proc_fs.h>
#endif /* CONFIG_PROC_FS */
#include <stdarg.h>

static unsigned long rtai_cpufreq_arg = CONFIG_CLOCK_FREQ;
RTAI_MODULE_PARM(rtai_cpufreq_arg, ulong);

#define RTAI_NR_IRQS  IPIPE_NR_XIRQS

//static int rtai_request_tickdev(void);

//static void rtai_release_tickdev(void);

#define rtai_setup_periodic_apic(count, vector)

#define rtai_setup_oneshot_apic(count, vector)

#define __ack_APIC_irq()

struct { volatile int locked, rqsted; } rt_scheduling[RTAI_NR_CPUS];

#ifdef CONFIG_RTAI_SCHED_ISR_LOCK
static void (*rtai_isr_hook)(int cpuid);
#endif /* CONFIG_RTAI_SCHED_ISR_LOCK */

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

static spinlock_t rtai_lsrq_lock = SPIN_LOCK_UNLOCKED;

static volatile int rtai_sync_level;

static atomic_t rtai_sync_count = ATOMIC_INIT(1);

static struct desc_struct rtai_sysvec;

static struct desc_struct rtai_cmpxchg_trap_vec;
static struct desc_struct rtai_xchg_trap_vec;

static RT_TRAP_HANDLER rtai_trap_handler;

struct rt_times rt_times;

struct rt_times rt_smp_times[RTAI_NR_CPUS];

struct rtai_switch_data rtai_linux_context[RTAI_NR_CPUS];

#if LINUX_VERSION_CODE < RTAI_LT_KERNEL_VERSION_FOR_NONPERCPU
volatile unsigned long *ipipe_root_status[RTAI_NR_CPUS];
#endif

struct calibration_data rtai_tunables;

volatile unsigned long rtai_cpu_realtime;

volatile unsigned long rtai_cpu_lock[2];

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
	rtai_realtime_irq[irq].irq_ack = hal_root_domain->irqs[irq].acknowledge;
	rtai_critical_exit(flags);
	if (IsolCpusMask && irq < IPIPE_NR_XIRQS) {
		rtai_realtime_irq[irq].cpumask = rt_assign_irq_to_cpu(irq, IsolCpusMask);
	}
	return 0;
}

int rt_release_irq (unsigned irq)
{
	unsigned long flags;
	if (irq >= RTAI_NR_IRQS || !rtai_realtime_irq[irq].handler) {
		return -EINVAL;
	}
	flags = rtai_critical_enter(NULL);
	rtai_realtime_irq[irq].handler = NULL;
	rtai_realtime_irq[irq].irq_ack = hal_root_domain->irqs[irq].acknowledge;
	rtai_critical_exit(flags);
	if (IsolCpusMask && irq < IPIPE_NR_XIRQS) {
		rt_assign_irq_to_cpu(irq, rtai_realtime_irq[irq].cpumask);
	}
	return 0;
}

//Timer staff
#include <asm/coldfire.h>
#include <asm/mcftimer.h>

static int timer_inuse=0;

int rt_ack_tmr(unsigned int irq)
{
	/* Reset the ColdFire timer */
	__raw_writeb(MCFTIMER_TER_CAP | MCFTIMER_TER_REF, TA(MCFTIMER_TER));

	/* keep emulated time stamp counter up to date */
	read_timer_cnt();
	return 0;
}

int rt_ack_uart(unsigned int irq)
{
	mcf_disable_irq0(irq);
	return 0;
}

int rt_set_irq_ack(unsigned irq, int (*irq_ack)(unsigned int))
{
	if (irq >= RTAI_NR_IRQS) {
		return -EINVAL;
	}
	rtai_realtime_irq[irq].irq_ack = irq_ack ? irq_ack : hal_root_domain->irqs[irq].acknowledge;
	return 0;
}

void rt_set_irq_cookie (unsigned irq, void *cookie)
{
	if (irq < RTAI_NR_IRQS) {
		rtai_realtime_irq[irq].cookie = cookie;
	}
}

void rt_set_irq_retmode (unsigned irq, int retmode)
{
	if (irq < RTAI_NR_IRQS) {
		rtai_realtime_irq[irq].retmode = retmode ? 1 : 0;
	}
}

extern unsigned long io_apic_irqs;

#if LINUX_VERSION_CODE >= RTAI_LT_KERNEL_VERSION_FOR_IRQDESC
#define rtai_irq_desc(irq) (irq_desc[irq].chip)
#endif

#define BEGIN_PIC()
#define END_PIC()
#undef hal_lock_irq
#undef hal_unlock_irq
#define hal_lock_irq(x, y, z)
#define hal_unlock_irq(x, y)

/**
 * start and initialize the PIC to accept interrupt request irq.
 *
 * The above function allow you to manipulate the PIC at hand, but you must
 * know what you are doing. Such a duty does not pertain to this manual and
 * you should refer to your PIC datasheet.
 *
 * Note that Linux has the same functions, but they must be used only for its
 * interrupts. Only the above ones can be safely used in real time handlers.
 *
 * It must also be remarked that when you install a real time interrupt handler,
 * RTAI already calls either rt_mask_and_ack_irq(), for level triggered
 * interrupts, or rt_ack_irq(), for edge triggered interrupts, before passing
 * control to you interrupt handler. hus generally you should just call
 * rt_unmask_irq() at due time, for level triggered interrupts, while nothing
 * should be done for edge triggered ones. Recall that in the latter case you
 * allow also any new interrupts on the same request as soon as you enable
 * interrupts at the CPU level.
 *
 * Often some of the above functions do equivalent things. Once more there is no
 * way of doing it right except by knowing the hardware you are manipulating.
 * Furthermore you must also remember that when you install a hard real time
 * handler the related interrupt is usually disabled, unless you are overtaking
 * one already owned by Linux which has been enabled by it.   Recall that if
 * have done it right, and interrupts do not show up, it is likely you have just
 * to rt_enable_irq() your irq.
 */
unsigned rt_startup_irq (unsigned irq)
{
#if LINUX_VERSION_CODE >= RTAI_LT_KERNEL_VERSION_FOR_IRQDESC
        int retval;

	BEGIN_PIC();
	hal_unlock_irq(hal_root_domain, irq);
	retval = rtai_irq_desc(irq)->startup(irq);
	END_PIC();
        return retval;
#else
	return 0;
#endif
}

/**
 * Shut down an IRQ source.
 *
 * No further interrupt request irq can be accepted.
 *
 * The above function allow you to manipulate the PIC at hand, but you must
 * know what you are doing. Such a duty does not pertain to this manual and
 * you should refer to your PIC datasheet.
 *
 * Note that Linux has the same functions, but they must be used only for its
 * interrupts. Only the above ones can be safely used in real time handlers.
 *
 * It must also be remarked that when you install a real time interrupt handler,
 * RTAI already calls either rt_mask_and_ack_irq(), for level triggered
 * interrupts, or rt_ack_irq(), for edge triggered interrupts, before passing
 * control to you interrupt handler. hus generally you should just call
 * rt_unmask_irq() at due time, for level triggered interrupts, while nothing
 * should be done for edge triggered ones. Recall that in the latter case you
 * allow also any new interrupts on the same request as soon as you enable
 * interrupts at the CPU level.
 *
 * Often some of the above functions do equivalent things. Once more there is no
 * way of doing it right except by knowing the hardware you are manipulating.
 * Furthermore you must also remember that when you install a hard real time
 * handler the related interrupt is usually disabled, unless you are overtaking
 * one already owned by Linux which has been enabled by it.   Recall that if
 * have done it right, and interrupts do not show up, it is likely you have just
 * to rt_enable_irq() your irq.
 */
void rt_shutdown_irq (unsigned irq)
{
#if LINUX_VERSION_CODE >= RTAI_LT_KERNEL_VERSION_FOR_IRQDESC
	BEGIN_PIC();
	rtai_irq_desc(irq)->shutdown(irq);
#if LINUX_VERSION_CODE < RTAI_LT_KERNEL_VERSION_FOR_NONPERCPU
	hal_clear_irq(hal_root_domain, irq);
#endif
	END_PIC();
#endif
}

static inline void _rt_enable_irq (unsigned irq)
{
#if LINUX_VERSION_CODE >= RTAI_LT_KERNEL_VERSION_FOR_IRQDESC
	BEGIN_PIC();
	hal_unlock_irq(hal_root_domain, irq);
	rtai_irq_desc(irq)->enable(irq);
	END_PIC();
#endif
}

/**
 * Enable an IRQ source.
 *
 * The above function allow you to manipulate the PIC at hand, but you must
 * know what you are doing. Such a duty does not pertain to this manual and
 * you should refer to your PIC datasheet.
 *
 * Note that Linux has the same functions, but they must be used only for its
 * interrupts. Only the above ones can be safely used in real time handlers.
 *
 * It must also be remarked that when you install a real time interrupt handler,
 * RTAI already calls either rt_mask_and_ack_irq(), for level triggered
 * interrupts, or rt_ack_irq(), for edge triggered interrupts, before passing
 * control to you interrupt handler. hus generally you should just call
 * rt_unmask_irq() at due time, for level triggered interrupts, while nothing
 * should be done for edge triggered ones. Recall that in the latter case you
 * allow also any new interrupts on the same request as soon as you enable
 * interrupts at the CPU level.
 *
 * Often some of the above functions do equivalent things. Once more there is no
 * way of doing it right except by knowing the hardware you are manipulating.
 * Furthermore you must also remember that when you install a hard real time
 * handler the related interrupt is usually disabled, unless you are overtaking
 * one already owned by Linux which has been enabled by it.   Recall that if
 * have done it right, and interrupts do not show up, it is likely you have just
 * to rt_enable_irq() your irq.
 */
void rt_enable_irq (unsigned irq)
{
	_rt_enable_irq(irq);
}

/**
 * Disable an IRQ source.
 *
 * The above function allow you to manipulate the PIC at hand, but you must
 * know what you are doing. Such a duty does not pertain to this manual and
 * you should refer to your PIC datasheet.
 *
 * Note that Linux has the same functions, but they must be used only for its
 * interrupts. Only the above ones can be safely used in real time handlers.
 *
 * It must also be remarked that when you install a real time interrupt handler,
 * RTAI already calls either rt_mask_and_ack_irq(), for level triggered
 * interrupts, or rt_ack_irq(), for edge triggered interrupts, before passing
 * control to you interrupt handler. hus generally you should just call
 * rt_unmask_irq() at due time, for level triggered interrupts, while nothing
 * should be done for edge triggered ones. Recall that in the latter case you
 * allow also any new interrupts on the same request as soon as you enable
 * interrupts at the CPU level.
 *
 * Often some of the above functions do equivalent things. Once more there is no
 * way of doing it right except by knowing the hardware you are manipulating.
 * Furthermore you must also remember that when you install a hard real time
 * handler the related interrupt is usually disabled, unless you are overtaking
 * one already owned by Linux which has been enabled by it.   Recall that if
 * have done it right, and interrupts do not show up, it is likely you have just
 * to rt_enable_irq() your irq.
 */
void rt_disable_irq (unsigned irq)
{
#if LINUX_VERSION_CODE >= RTAI_LT_KERNEL_VERSION_FOR_IRQDESC
	BEGIN_PIC();
	rtai_irq_desc(irq)->disable(irq);
	hal_lock_irq(hal_root_domain, 0, irq);
	END_PIC();
#endif
}

/**
 * Mask and acknowledge and IRQ source.
 *
 * No  * other interrupts can be accepted, once also the CPU will enable
 * interrupts, which ones depends on the PIC at hand and on how it is
 * programmed.
 *
 * The above function allow you to manipulate the PIC at hand, but you must
 * know what you are doing. Such a duty does not pertain to this manual and
 * you should refer to your PIC datasheet.
 *
 * Note that Linux has the same functions, but they must be used only for its
 * interrupts. Only the above ones can be safely used in real time handlers.
 *
 * It must also be remarked that when you install a real time interrupt handler,
 * RTAI already calls either rt_mask_and_ack_irq(), for level triggered
 * interrupts, or rt_ack_irq(), for edge triggered interrupts, before passing
 * control to you interrupt handler. hus generally you should just call
 * rt_unmask_irq() at due time, for level triggered interrupts, while nothing
 * should be done for edge triggered ones. Recall that in the latter case you
 * allow also any new interrupts on the same request as soon as you enable
 * interrupts at the CPU level.
 *
 * Often some of the above functions do equivalent things. Once more there is no
 * way of doing it right except by knowing the hardware you are manipulating.
 * Furthermore you must also remember that when you install a hard real time
 * handler the related interrupt is usually disabled, unless you are overtaking
 * one already owned by Linux which has been enabled by it.   Recall that if
 * have done it right, and interrupts do not show up, it is likely you have just
 * to rt_enable_irq() your irq.
 */
void rt_mask_and_ack_irq (unsigned irq)
{
#if LINUX_VERSION_CODE >= RTAI_LT_KERNEL_VERSION_FOR_IRQDESC
	rtai_irq_desc(irq)->ack(irq);
#endif
}

static inline void _rt_end_irq (unsigned irq)
{
#if LINUX_VERSION_CODE >= RTAI_LT_KERNEL_VERSION_FOR_IRQDESC
	BEGIN_PIC();
	if (
	    !(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS))) {
		hal_unlock_irq(hal_root_domain, irq);
	}
	rtai_irq_desc(irq)->end(irq);
	END_PIC();
#endif
}

/**
 * Unmask and IRQ source.
 *
 * The related request can then interrupt the CPU again, provided it has also
 * been acknowledged.
 *
 * The above function allow you to manipulate the PIC at hand, but you must
 * know what you are doing. Such a duty does not pertain to this manual and
 * you should refer to your PIC datasheet.
 *
 * Note that Linux has the same functions, but they must be used only for its
 * interrupts. Only the above ones can be safely used in real time handlers.
 *
 * It must also be remarked that when you install a real time interrupt handler,
 * RTAI already calls either rt_mask_and_ack_irq(), for level triggered
 * interrupts, or rt_ack_irq(), for edge triggered interrupts, before passing
 * control to you interrupt handler. hus generally you should just call
 * rt_unmask_irq() at due time, for level triggered interrupts, while nothing
 * should be done for edge triggered ones. Recall that in the latter case you
 * allow also any new interrupts on the same request as soon as you enable
 * interrupts at the CPU level.
 *
 * Often some of the above functions do equivalent things. Once more there is no
 * way of doing it right except by knowing the hardware you are manipulating.
 * Furthermore you must also remember that when you install a hard real time
 * handler the related interrupt is usually disabled, unless you are overtaking
 * one already owned by Linux which has been enabled by it.   Recall that if
 * have done it right, and interrupts do not show up, it is likely you have just
 * to rt_enable_irq() your irq.
 */
void rt_unmask_irq (unsigned irq)
{
	_rt_end_irq(irq);
}

/**
 * Acknowledge an IRQ source.
 *
 * The related request can then interrupt the CPU again, provided it has not
 * been masked.
 *
 * The above function allow you to manipulate the PIC at hand, but you must
 * know what you are doing. Such a duty does not pertain to this manual and
 * you should refer to your PIC datasheet.
 *
 * Note that Linux has the same functions, but they must be used only for its
 * interrupts. Only the above ones can be safely used in real time handlers.
 *
 * It must also be remarked that when you install a real time interrupt handler,
 * RTAI already calls either rt_mask_and_ack_irq(), for level triggered
 * interrupts, or rt_ack_irq(), for edge triggered interrupts, before passing
 * control to you interrupt handler. hus generally you should just call
 * rt_unmask_irq() at due time, for level triggered interrupts, while nothing
 * should be done for edge triggered ones. Recall that in the latter case you
 * allow also any new interrupts on the same request as soon as you enable
 * interrupts at the CPU level.
 *
 * Often some of the above functions do equivalent things. Once more there is no
 * way of doing it right except by knowing the hardware you are manipulating.
 * Furthermore you must also remember that when you install a hard real time
 * handler the related interrupt is usually disabled, unless you are overtaking
 * one already owned by Linux which has been enabled by it.   Recall that if
 * have done it right, and interrupts do not show up, it is likely you have just
 * to rt_enable_irq() your irq.
 */
void rt_ack_irq (unsigned irq)
{
	_rt_enable_irq(irq);
}

void rt_end_irq (unsigned irq)
{
	_rt_end_irq(irq);
}

void rt_eoi_irq (unsigned irq)
{
#if LINUX_VERSION_CODE >= RTAI_LT_KERNEL_VERSION_FOR_IRQDESC
        BEGIN_PIC();
        if (
            !(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS))) {
                hal_unlock_irq(hal_root_domain, irq);
        }
        rtai_irq_desc(irq)->end(irq);
        END_PIC();
#endif
}

/**
 * Install shared Linux interrupt handler.
 *
 * rt_request_linux_irq installs function @a handler as a standard Linux
 * interrupt service routine for IRQ level @a irq forcing Linux to share the IRQ
 * with other interrupt handlers, even if it does not want. The handler is
 * appended to any already existing Linux handler for the same irq and is run by
 * Linux irq as any of its handler. In this way a real time application can
 * monitor Linux interrupts handling at its will. The handler appears in
 * /proc/interrupts.
 *
 * @param handler pointer on the interrupt service routine to be installed.
 *
 * @param name is a name for /proc/interrupts.
 *
 * @param dev_id is to pass to the interrupt handler, in the same way as the
 * standard Linux irq request call.
 *
 * The interrupt service routine can be uninstalled with rt_free_linux_irq().
 *
 * @retval 0 on success.
 * @retval EINVAL if @a irq is not a valid IRQ number or handler is @c NULL.
 * @retval EBUSY if there is already a handler of interrupt @a irq.
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

	return retval;
}

/**
 * Uninstall shared Linux interrupt handler.
 *
 * @param dev_id is to pass to the interrupt handler, in the same way as the
 * standard Linux irq request call.
 *
 * @param irq is the IRQ level of the interrupt handler to be freed.
 *
 * @retval 0 on success.
 * @retval EINVAL if @a irq is not a valid IRQ number.
 */
int rt_free_linux_irq (unsigned irq, void *dev_id)
{
	unsigned long flags;

	if (irq >= RTAI_NR_IRQS || rtai_linux_irq[irq].count == 0) {
		return -EINVAL;
	}

	rtai_save_flags_and_cli(flags);
	free_irq(irq,dev_id);
	--rtai_linux_irq[irq].count;
	rtai_restore_flags(flags);

	return 0;
}

/**
 * Pend an IRQ to Linux.
 *
 * rt_pend_linux_irq appends a Linux interrupt irq for processing in Linux IRQ
 * mode, i.e. with hardware interrupts fully enabled.
 *
 * @note rt_pend_linux_irq does not perform any check on @a irq.
 */
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

/**
 * Install a system request handler
 *
 * rt_request_srq installs a two way RTAI system request (srq) by assigning
 * @a u_handler, a function to be used when a user calls srq from user space,
 * and @a k_handler, the function to be called in kernel space following its
 * activation by a call to rt_pend_linux_srq(). @a k_handler is in practice
 * used to request a service from the kernel. In fact Linux system requests
 * cannot be used safely from RTAI so you can setup a handler that receives real
 * time requests and safely executes them when Linux is running.
 *
 * @param u_handler can be used to effectively enter kernel space without the
 * overhead and clumsiness of standard Unix/Linux protocols.   This is very
 * flexible service that allows you to personalize your use of  RTAI.
 *
 * @return the number of the assigned system request on success.
 * @retval EINVAL if @a k_handler is @c NULL.
 * @retval EBUSY if no free srq slot is available.
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

/**
 * Uninstall a system request handler
 *
 * rt_free_srq uninstalls the specified system call @a srq, returned by
 * installing the related handler with a previous call to rt_request_srq().
 *
 * @retval EINVAL if @a srq is invalid.
 */
int rt_free_srq (unsigned srq)
{
	return  (srq < 1 || srq >= RTAI_NR_SRQS || !test_and_clear_bit(srq, &rtai_sysreq_map)) ? -EINVAL : 0;
}

/**
 * Append a Linux IRQ.
 *
 * rt_pend_linux_srq appends a system call request srq to be used as a service
 * request to the Linux kernel.
 *
 * @param srq is the value returned by rt_request_srq.
 *
 * @note rt_pend_linux_srq does not perform any check on irq.
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

irqreturn_t rtai_broadcast_to_local_timers (int irq, void *dev_id, struct pt_regs *regs)
{
	return RTAI_LINUX_IRQ_HANDLED;
}

#define REQUEST_LINUX_IRQ_BROADCAST_TO_APIC_TIMERS()  0

#define FREE_LINUX_IRQ_BROADCAST_TO_APIC_TIMERS();

#ifdef CONFIG_RTAI_SCHED_ISR_LOCK
#define RTAI_SCHED_ISR_LOCK() \
	do { \
		if (!rt_scheduling[0].locked++) { \
			rt_scheduling[0].rqsted = 0; \
		} \
	} while (0)
#define RTAI_SCHED_ISR_UNLOCK() \
	do { \
		if (rt_scheduling[0].locked && !(--rt_scheduling[0].locked)) { \
			if (rt_scheduling[0].rqsted > 0 && rtai_isr_hook) { \
				rtai_isr_hook(0); \
        		} \
		} \
	} while (0)
#else  /* !CONFIG_RTAI_SCHED_ISR_LOCK */
#define RTAI_SCHED_ISR_LOCK() \
	do { cpuid = 0; } while (0)
#define RTAI_SCHED_ISR_UNLOCK() \
	do {                       } while (0)
#endif /* CONFIG_RTAI_SCHED_ISR_LOCK */

#define HAL_TICK_REGS hal_tick_regs[cpuid]

#ifdef LOCKED_LINUX_IN_IRQ_HANDLER
#define HAL_LOCK_LINUX()  do { sflags = rt_save_switch_to_real_time(cpuid); } while (0)
#define HAL_UNLOCK_LINUX()  do { rtai_cli(); rt_restore_switch_to_linux(sflags, cpuid); } while (0)
#else
#define HAL_LOCK_LINUX()  do { sflags = xchg(ROOT_STATUS_ADR(cpuid), (1 << IPIPE_STALL_FLAG)); } while (0)
#define HAL_UNLOCK_LINUX()  do { rtai_cli(); ROOT_STATUS_VAL(cpuid) = sflags; } while (0)
#endif

#ifndef STR
#define __STR(x) #x
#define STR(x) __STR(x)
#endif

#ifndef SYMBOL_NAME_STR
#define SYMBOL_NAME_STR(X) #X
#endif

#define SAVE_REG \
       "move   #0x2700,%sr\n\t"                /* disable intrs */ \
       "btst   #5,%sp@(2)\n\t"                 /* from user? */ \
       "bnes   6f\n\t"                         /* no, skip */ \
       "movel  %sp,sw_usp\n\t"                 /* save user sp */ \
       "addql  #8,sw_usp\n\t"                  /* remove exception */ \
       "movel  sw_ksp,%sp\n\t"                 /* kernel sp */ \
       "subql  #8,%sp\n\t"                     /* room for exception */ \
       "clrl   %sp@-\n\t"                      /* stkadj */ \
       "movel  %d0,%sp@-\n\t"                  /* orig d0 */ \
       "movel  %d0,%sp@-\n\t"                  /* d0 */ \
       "lea    %sp@(-32),%sp\n\t"              /* space for 8 regs */ \
       "moveml %d1-%d5/%a0-%a2,%sp@\n\t" \
       "movel  sw_usp,%a0\n\t"                 /* get usp */ \
       "movel  %a0@-,%sp@(48)\n\t"             /* copy exception program counter (PT_PC=48)*/ \
       "movel  %a0@-,%sp@(44)\n\t"     /* copy exception format/vector/sr (PT_FORMATVEC=44)*/ \
       "bra    7f\n\t" \
       "6:\n\t" \
       "clrl   %sp@-\n\t"                      /* stkadj */ \
       "movel  %d0,%sp@-\n\t"                  /* orig d0 */ \
       "movel  %d0,%sp@-\n\t"                  /* d0 */ \
       "lea    %sp@(-32),%sp\n\t"              /* space for 8 regs */ \
       "moveml %d1-%d5/%a0-%a2,%sp@\n\t" \
       "7:\n\t" \
       "move   #0x2000,%sr\n\t"

#define RSTR_REG \
       "btst   #5,%sp@(46)\n\t"                /* going user? (PT_SR=46)*/ \
       "bnes   8f\n\t"                         /* no, skip */ \
       "move   #0x2700,%sr\n\t"                /* disable intrs */ \
       "movel  sw_usp,%a0\n\t"                 /* get usp */ \
       "movel  %sp@(48),%a0@-\n\t"             /* copy exception program counter (PT_PC=48)*/ \
       "movel  %sp@(44),%a0@-\n\t"     /* copy exception format/vector/sr (PT_FORMATVEC=44)*/ \
       "moveml %sp@,%d1-%d5/%a0-%a2\n\t" \
       "lea    %sp@(32),%sp\n\t"               /* space for 8 regs */ \
       "movel  %sp@+,%d0\n\t" \
       "addql  #4,%sp\n\t"                     /* orig d0 */ \
       "addl   %sp@+,%sp\n\t"                  /* stkadj */ \
       "addql  #8,%sp\n\t"                     /* remove exception */ \
       "movel  %sp,sw_ksp\n\t"                 /* save ksp */ \
       "subql  #8,sw_usp\n\t"                  /* set exception */ \
       "movel  sw_usp,%sp\n\t"                 /* restore usp */ \
       "rte\n\t" \
       "8:\n\t" \
       "moveml %sp@,%d1-%d5/%a0-%a2\n\t" \
       "lea    %sp@(32),%sp\n\t"               /* space for 8 regs */ \
       "movel  %sp@+,%d0\n\t" \
       "addql  #4,%sp\n\t"                     /* orig d0 */ \
       "addl   %sp@+,%sp\n\t"                  /* stkadj */ \
       "rte"

#define DEFINE_VECTORED_ISR(name, fun) \
	__asm__ ( \
        	SYMBOL_NAME_STR(name) ":\n\t" \
		SAVE_REG \
		"jsr "SYMBOL_NAME_STR(fun)"\n\t" \
		RSTR_REG);

#define rtai_critical_sync NULL

int rt_assign_irq_to_cpu (int irq, unsigned long cpus_mask)
{
	return 0;
}

int rt_reset_irq_to_sym_mode (int irq)
{
	return 0;
}

extern void mcf_settimericr(int timer, int level);

/**
 * Install a timer interrupt handler.
 *
 * rt_request_timer requests a timer of period tick ticks, and installs the
 * routine @a handler as a real time interrupt service routine for the timer.
 *
 * Set @a tick to 0 for oneshot mode (in oneshot mode it is not used).
 *
 */

int rt_request_timer (void (*handler)(void), unsigned tick, int unused)
{
	unsigned long flags;
	int retval;

	TRACE_RTAI_TIMER(TRACE_RTAI_EV_TIMER_REQUEST,handler,tick);
	if (timer_inuse) return -EINVAL;
	timer_inuse = 1;

	rtai_save_flags_and_cli(flags);


   	if (tick > 0)
	{
		rt_times.linux_tick = LATCH;
		rt_times.tick_time = read_timer_cnt();
		rt_times.intr_time = rt_times.tick_time + tick;
		rt_times.linux_time = rt_times.tick_time + rt_times.linux_tick;
		rt_times.periodic_tick = tick;

		rt_set_timer_delay(tick);
	}
	else
	{
		rt_times.tick_time = rdtsc();
		rt_times.linux_tick = imuldiv(LATCH,rtai_tunables.cpu_freq,RTAI_FREQ_8254);
		rt_times.intr_time = rt_times.tick_time + rt_times.linux_tick;
		rt_times.linux_time = rt_times.tick_time + rt_times.linux_tick;
		rt_times.periodic_tick = rt_times.linux_tick;

		rt_set_timer_delay(LATCH);
	}
	
	retval = rt_request_global_irq(RT_TIMER_IRQ, handler);
	rt_set_irq_ack(RT_TIMER_IRQ, rt_ack_tmr);
	rtai_restore_flags(flags);
	return retval;
}

/**
 * Uninstall a timer interrupt handler.
 *
 * rt_free_timer uninstalls a timer previously set by rt_request_timer().
 */
void rt_free_timer (void)
{
	unsigned long flags;

	TRACE_RTAI_TIMER(TRACE_RTAI_EV_TIMER_FREE,0,0);

	rtai_save_flags_and_cli(flags);
	timer_inuse = 0;

	rt_free_global_irq(RT_TIMER_IRQ);	

	rtai_restore_flags(flags);
}

long long rdtsc()
{
	return read_timer_cnt() * (tuned.cpu_freq / TIMER_FREQ);
}

RT_TRAP_HANDLER rt_set_trap_handler (RT_TRAP_HANDLER handler)
{
	return (RT_TRAP_HANDLER)xchg(&rtai_trap_handler, handler);
}

#define CHECK_KERCTX();

int rtai_8254_timer_handler(struct pt_regs regs);

static int rtai_hirq_dispatcher (unsigned irq, struct pt_regs *regs)
{
	unsigned long cpuid = 0;

	CHECK_KERCTX();
	
	if (rtai_realtime_irq[irq].handler) {
		unsigned long sflags;
		if (irq == RT_TIMER_IRQ)
		{
			unsigned long sflags = 0;
			HAL_LOCK_LINUX();
			RTAI_SCHED_ISR_LOCK();
			if (rtai_realtime_irq[RT_TIMER_IRQ].irq_ack)
				rtai_realtime_irq[RT_TIMER_IRQ].irq_ack(RT_TIMER_IRQ);
			((void (*)(void))rtai_realtime_irq[RT_TIMER_IRQ].handler)();
			RTAI_SCHED_ISR_UNLOCK();
			HAL_UNLOCK_LINUX();
			if (!test_bit(IPIPE_STALL_FLAG, ROOT_STATUS_ADR(cpuid)))  {
				rtai_sti();
#if LINUX_VERSION_CODE < RTAI_LT_KERNEL_VERSION_FOR_NONPERCPU
				HAL_TICK_REGS.sr = regs->sr;
				HAL_TICK_REGS.pc = regs->pc;
#else
				__raw_get_cpu_var(__ipipe_tick_regs).sr = regs->sr;
				__raw_get_cpu_var(__ipipe_tick_regs).pc = regs->pc;
#endif
				hal_fast_flush_pipeline(cpuid);
				return 1;
			}
			return 0;
		}

		HAL_LOCK_LINUX();
		if (rtai_realtime_irq[irq].irq_ack)
			rtai_realtime_irq[irq].irq_ack(irq);
		mb();
		RTAI_SCHED_ISR_LOCK();
		if (rtai_realtime_irq[irq].retmode && rtai_realtime_irq[irq].handler(irq, rtai_realtime_irq[irq].cookie)) {
			RTAI_SCHED_ISR_UNLOCK();
			HAL_UNLOCK_LINUX();
			return 0;
		} else {
			rtai_realtime_irq[irq].handler(irq, rtai_realtime_irq[irq].cookie);
			RTAI_SCHED_ISR_UNLOCK();
			HAL_UNLOCK_LINUX();
			if (test_bit(IPIPE_STALL_FLAG, ROOT_STATUS_ADR(cpuid))) {
				return 0;
			}
		}
	} else {
		unsigned long lflags;
		cpuid = rtai_cpuid();
//		lflags = xchg(ROOT_STATUS_ADR(cpuid), (1 << IPIPE_STALL_FLAG));
		lflags = ROOT_STATUS_VAL(cpuid);
		ROOT_STATUS_VAL(cpuid) = (1 << IPIPE_STALL_FLAG);
		if (rtai_realtime_irq[irq].irq_ack)
			rtai_realtime_irq[irq].irq_ack(irq);
		mb();
		hal_pend_uncond(irq, cpuid);
		ROOT_STATUS_VAL(cpuid) = lflags;

		if (test_bit(IPIPE_STALL_FLAG, &lflags)) {
			return 0;
		}
	}

	if (irq == hal_tick_irq) {
#if LINUX_VERSION_CODE < RTAI_LT_KERNEL_VERSION_FOR_NONPERCPU
		HAL_TICK_REGS.sr = regs->sr;
		HAL_TICK_REGS.pc = regs->pc;
#else
		__raw_get_cpu_var(__ipipe_tick_regs).sr = regs->sr;
		__raw_get_cpu_var(__ipipe_tick_regs).pc = regs->pc;
#endif
	}
	rtai_sti();
	hal_fast_flush_pipeline(cpuid);
	return 1;
}

#ifdef HINT_DIAG_ECHO
#define HINT_DIAG_MSG(x) x
#else
#define HINT_DIAG_MSG(x)
#endif

static int PrintFpuTrap = 0;
RTAI_MODULE_PARM(PrintFpuTrap, int);
static int PrintFpuInit = 0;
RTAI_MODULE_PARM(PrintFpuInit, int);

static int rtai_trap_fault (unsigned event, void *evdata)
{
#ifdef HINT_DIAG_TRAPS
	static unsigned long traps_in_hard_intr = 0;
        do {
                unsigned long flags;
                rtai_save_flags_and_cli(flags);
                if (flags & ~ALLOWINT) {
                        if (!test_and_set_bit(event, &traps_in_hard_intr)) {
                                HINT_DIAG_MSG(rt_printk("TRAP %d HAS INTERRUPT DISABLED (TRAPS PICTURE %lx).\n", event, traps_in_hard_intr););
                        }
                }
        } while (0);
#endif

	static const int trap2sig[] = {
                       0,              //  0 - Initial stack pointer
                       0,              //  1 - Initial program counter
                       SIGSEGV,        //  2 - Access error
                       SIGBUS,         //  3 - Address error
                       SIGILL,         //  4 - Illegal instruction
                       SIGFPE,         //  5 - Divide by zero
                       SIGFPE,         //  6 - Reserved
                       SIGFPE,         //  7 - Reserved
                       SIGILL,         //  8 - Priviledge violation
                       SIGTRAP,        //  9 - Trace
                       SIGILL,         // 10 - Unimplemented line-a opcode
                       SIGILL,         // 11 - Unimplemented line-f opcode
                       SIGILL,         // 12 - Non-PC breakpoint debug interrupt
                       SIGILL,         // 13 - PC breakpoint debug interrupt
                       SIGILL,         // 14 - Format error
                       SIGILL,         // 15 - Uninitialized interrupt
                       SIGILL,         // 16 - Reserved
                       SIGILL,         // 17 - Reserved
                       SIGILL,         // 18 - Reserved
                       SIGILL,         // 19 - Reserved
                       SIGILL,         // 20 - Reserved
                       SIGILL,         // 21 - Reserved
                       SIGILL,         // 22 - Reserved
                       SIGILL,         // 23 - Reserved
                       SIGILL,         // 24 - Spurious interrupt ?
                       0,              // 25
                       0,              // 26
                       0,              // 27
                       0,              // 28
                       0,              // 29
                       0,              // 30
                       0,              // 31
                       0,              // 32 - Trap 0 (syscall)
                       SIGTRAP,        // 33 - Trap 1 (gdbserver breakpoint)
                       SIGILL,         // 34 - Trap 2
                       SIGILL,         // 35 - Trap 3
                       SIGILL,         // 36 - Trap 4
                       SIGILL,         // 37 - Trap 5
                       SIGILL,         // 38 - Trap 6
                       SIGILL,         // 39 - Trap 7
                       SIGILL,         // 40 - Trap 8
                       SIGILL,         // 41 - Trap 9
                       SIGILL,         // 42 - Trap 10
                       SIGILL,         // 43 - Trap 11
                       SIGILL,         // 44 - Trap 12
                       SIGILL,         // 45 - Trap 13
                       SIGILL,         // 46 - Trap 14
                       SIGTRAP,        // 47 - Trap 15
                       SIGFPE,         // 48
                       SIGFPE,         // 49
                       SIGFPE,         // 50
                       SIGFPE,         // 51
                       SIGFPE,         // 52
                       SIGFPE,         // 53
                       SIGFPE,         // 54
                       SIGILL,         // 55
                       SIGILL,         // 56
                       SIGILL,         // 57
                       SIGILL,         // 58
                       SIGILL,         // 59
                       SIGILL,         // 60
                       SIGILL,         // 61
                       SIGILL,         // 62
                       SIGFPE          // 63
	};

	TRACE_RTAI_TRAP_ENTRY(evinfo->event, 0);

	if (!in_hrt_mode(rtai_cpuid())) {
		goto propagate;
	}

	if (rtai_trap_handler && rtai_trap_handler(event, trap2sig[event], (struct pt_regs *)evdata, NULL)) {
		goto endtrap;
	}
propagate:
	return 0;
endtrap:
	TRACE_RTAI_TRAP_EXIT();
	return 1;
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

#include <asm/rtai_usi.h>
long long (*rtai_lxrt_dispatcher)(unsigned long, unsigned long);

static int (*sched_intercept_syscall_prologue)(struct pt_regs *);

static int intercept_syscall_prologue(unsigned long event, struct pt_regs *regs)
{
	if (likely(regs->LINUX_SYSCALL_NR >= RTAI_SYSCALL_NR)) {
		unsigned long srq  = regs->LINUX_SYSCALL_REG1;
		IF_IS_A_USI_SRQ_CALL_IT(srq, regs->LINUX_SYSCALL_REG2, (long long *)regs->LINUX_SYSCALL_REG3, regs->LINUX_SYSCALL_FLAGS, 1);
		*((long long *)regs->LINUX_SYSCALL_REG3) = srq > RTAI_NR_SRQS ? rtai_lxrt_dispatcher(srq, regs->LINUX_SYSCALL_REG2) : rtai_usrq_dispatcher(srq, regs->LINUX_SYSCALL_REG2);
		if (!in_hrt_mode(srq = rtai_cpuid())) {
			hal_test_and_fast_flush_pipeline(srq);
			return 0;
		}
		return 1;
	}
	return likely(sched_intercept_syscall_prologue != NULL) ? sched_intercept_syscall_prologue(regs) : 0;
}

inline int usi_SRQ_call(unsigned long srq, unsigned long args, long long* result, unsigned long lsr)
{
	IF_IS_A_USI_SRQ_CALL_IT(srq, args, result, lsr, 1); //It will return 1 on success.
	return 0;
}

//We have: d0 - srq, d1 - args, d2 - retval
asmlinkage int rtai_syscall_dispatcher (__volatile struct pt_regs pt)
{
	int cpuid;
	//unsigned long lsr = pt.sr;
	long long result;
	//IF_IS_A_USI_SRQ_CALL_IT(pt.d0, pt.d1, (long long*)pt.d2, lsr, 0);
	if (usi_SRQ_call(pt.d0, pt.d1, &result, pt.sr))
		return 0;
	result = pt.d0 > RTAI_NR_SRQS ? rtai_lxrt_dispatcher(pt.d0, pt.d1) : rtai_usrq_dispatcher(pt.d0, pt.d1);
	pt.d2 = result & 0xFFFFFFFF;
	pt.d3 = (result >> 32);
	if (!in_hrt_mode(cpuid = rtai_cpuid())) {
		hal_test_and_fast_flush_pipeline(cpuid);
		return 1;
	}
	return 0;
}

void rtai_uvec_handler(void);
DEFINE_VECTORED_ISR(rtai_uvec_handler, rtai_syscall_dispatcher);

extern void *_ramvec;

struct desc_struct rtai_set_gate_vector (unsigned vector, int type, int dpl, void *handler)
{
	struct desc_struct* vector_table = (struct desc_struct*)_ramvec;
	struct desc_struct idt_element = vector_table[vector];
	vector_table[vector].a = handler;
	return idt_element;
}

void rtai_cmpxchg_trap_handler(void);
__asm__ ( \
	"rtai_cmpxchg_trap_handler:\n\t" \
	"move   #0x2700,%sr\n\t" \
	"movel	%a1@, %d0\n\t" \
	"cmpl	%d0,%d2\n\t" \
	"bnes	1f\n\t" \
	"movel	%d3,%a1@\n\t" \
	"1:\n\t" \
	"rte");

void rtai_xchg_trap_handler(void);
__asm__ ( \
	"rtai_xchg_trap_handler:\n\t" \
	"move   #0x2700,%sr\n\t" \
	"movel	%a1@, %d0\n\t" \
	"movel	%d2,%a1@\n\t" \
	"rte");

void rtai_reset_gate_vector (unsigned vector, struct desc_struct e)
{
       struct desc_struct* vector_table = (struct desc_struct*)_ramvec;
       vector_table[vector] = e;
}

static void rtai_install_archdep (void)
{

	unsigned long flags;

	flags = rtai_critical_enter(NULL);
    /* Backup and replace the sysreq vector. */
	rtai_sysvec = rtai_set_gate_vector(RTAI_SYS_VECTOR, 15, 3, &rtai_uvec_handler);
	rtai_cmpxchg_trap_vec = rtai_set_gate_vector(RTAI_CMPXCHG_TRAP_SYS_VECTOR, 15, 3, &rtai_cmpxchg_trap_handler);
	rtai_xchg_trap_vec = rtai_set_gate_vector(RTAI_XCHG_TRAP_SYS_VECTOR, 15, 3, &rtai_xchg_trap_handler);
	rtai_critical_exit(flags);

	hal_catch_event(hal_root_domain, HAL_SYSCALL_PROLOGUE, (void *)intercept_syscall_prologue);

	if (rtai_cpufreq_arg == 0) {
		struct hal_sysinfo_struct sysinfo;
		hal_get_sysinfo(&sysinfo);
		rtai_cpufreq_arg = (unsigned long)sysinfo.cpufreq;
	}
	rtai_tunables.cpu_freq = rtai_cpufreq_arg;
}

static void rtai_uninstall_archdep(void)
{
	unsigned long flags;

	flags = rtai_critical_enter(NULL);
	rtai_reset_gate_vector(RTAI_SYS_VECTOR, rtai_sysvec);
	rtai_reset_gate_vector(RTAI_CMPXCHG_TRAP_SYS_VECTOR, rtai_cmpxchg_trap_vec);
	rtai_reset_gate_vector(RTAI_XCHG_TRAP_SYS_VECTOR, rtai_xchg_trap_vec);
	rtai_critical_exit(flags);

	hal_catch_event(hal_root_domain, HAL_SYSCALL_PROLOGUE, NULL);
}

//Unimplemented
int rtai_calibrate_8254 (void)
{
	rt_printk("RTAI WARNING: rtai_calibrate_8254() isn't implemented for Coldfire\n");
	return 0;
}

void (*rt_set_ihook (void (*hookfn)(int)))(int)
{
#ifdef CONFIG_RTAI_SCHED_ISR_LOCK
	return (void (*)(int))xchg(&rtai_isr_hook, hookfn); /* This is atomic */
#else  /* !CONFIG_RTAI_SCHED_ISR_LOCK */
	return NULL;
#endif /* CONFIG_RTAI_SCHED_ISR_LOCK */
}

void rtai_set_linux_task_priority (struct task_struct *task, int policy, int prio)
{
	hal_set_linux_task_priority(task, policy, prio);
	if (task->rt_priority != prio || task->policy != policy) {
		printk("RTAI[hal]: sched_setscheduler(policy = %d, prio = %d) failed, (%s -- pid = %d)\n", policy, prio, task->comm, task->pid);
	}
}

#ifdef CONFIG_PROC_FS

struct proc_dir_entry *rtai_proc_root = NULL;

static int rtai_read_proc (char *page, char **start, off_t off, int count, int *eof, void *data)
{
	PROC_PRINT_VARS;
	int i, none;

	PROC_PRINT("\n** RTAI/m68knommu:\n\n");
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
	PROC_PRINT("    SYSREQ=0x%x\n\n", RTAI_SYS_VECTOR);

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

	PROC_PRINT_DONE;
}

static int rtai_proc_register (void)
{
	struct proc_dir_entry *ent;

	rtai_proc_root = create_proc_entry("rtai",S_IFDIR, 0);
	if (!rtai_proc_root) {
		printk(KERN_ERR "Unable to initialize /proc/rtai.\n");
		return -1;
        }
	rtai_proc_root->owner = THIS_MODULE;
	ent = create_proc_entry("hal",S_IFREG|S_IRUGO|S_IWUSR,rtai_proc_root);
	if (!ent) {
		printk(KERN_ERR "Unable to initialize /proc/rtai/hal.\n");
		return -1;
        }
	ent->read_proc = rtai_read_proc;

	return 0;
}

static void rtai_proc_unregister (void)
{
	remove_proc_entry("hal",rtai_proc_root);
	remove_proc_entry("rtai",0);
}

#endif /* CONFIG_PROC_FS */

FIRST_LINE_OF_RTAI_DOMAIN_ENTRY
{
	{
		rt_printk(KERN_INFO "RTAI[hal]: <%s> mounted over %s %s.\n", PACKAGE_VERSION, HAL_TYPE, HAL_VERSION_STRING);
		rt_printk(KERN_INFO "RTAI[hal]: compiled with %s.\n", CONFIG_RTAI_COMPILER);
	}
	local_irq_disable_hw();
	for (;;) hal_suspend_domain();
}
LAST_LINE_OF_RTAI_DOMAIN_ENTRY

long rtai_catch_event (struct hal_domain_struct *from, unsigned long event, int (*handler)(unsigned long, void *))
{
	if (event == HAL_SYSCALL_PROLOGUE) {
		sched_intercept_syscall_prologue = (void *)handler;
		return 0;
	}
	return (long)hal_catch_event(from, event, (void *)handler);
}

static void *saved_hal_irq_handler;
extern void *hal_irq_handler;

#undef ack_bad_irq
void ack_bad_irq(unsigned int irq)
{
	printk("unexpected IRQ trap at vector %02x\n", irq);
}

int __rtai_hal_init (void)
{
	int trapnr, halinv = 0;
	struct hal_attr_struct attr;

	for (halinv = trapnr = 0; trapnr < HAL_NR_EVENTS; trapnr++) {
		if (hal_root_domain->hal_event_handler_fun(trapnr)) {
			halinv = 1;
			printk("EVENT %d INVALID.\n", trapnr);
		}
	}
	if (halinv) {
		printk(KERN_ERR "RTAI[hal]: HAL IMMEDIATE EVENT DISPATCHING BROKEN.\n");
	}

	if (!(rtai_sysreq_virq = hal_alloc_irq())) {
		printk(KERN_ERR "RTAI[hal]: NO VIRTUAL INTERRUPT AVAILABLE.\n");
		halinv = 1;
	}
	
	if (halinv) {
		return -1;
	}

	for (trapnr = 0; trapnr < RTAI_NR_IRQS; trapnr++) {
		rtai_realtime_irq[trapnr].irq_ack = hal_root_domain->irqs[trapnr].acknowledge;
	}
//	rt_set_irq_ack(mcf_timervector, ack_linux_tmr);
//	rt_set_irq_ack(mcf_uartvector, ack_linux_uart);
#if LINUX_VERSION_CODE < RTAI_LT_KERNEL_VERSION_FOR_NONPERCPU
	for (trapnr = 0; trapnr < num_online_cpus(); trapnr++) {
		ipipe_root_status[trapnr] = &hal_root_domain->cpudata[trapnr].status;
	}
#endif

	hal_virtualize_irq(hal_root_domain, rtai_sysreq_virq, &rtai_lsrq_dispatcher, NULL, IPIPE_HANDLE_MASK);
	saved_hal_irq_handler = hal_irq_handler;
	hal_irq_handler = rtai_hirq_dispatcher;

	rtai_install_archdep();

#ifdef CONFIG_PROC_FS
	rtai_proc_register();
#endif

	hal_init_attr(&attr);
	attr.name     = "RTAI";
	attr.domid    = RTAI_DOMAIN_ID;
	attr.entry    = (void *)rtai_domain_entry;
	attr.priority = get_domain_pointer(1)->priority + 100;
	hal_register_domain(&rtai_domain, &attr);
	for (trapnr = 0; trapnr < HAL_NR_FAULTS; trapnr++) {
		hal_catch_event(hal_root_domain, trapnr, (void *)rtai_trap_fault);
	}
	rtai_init_taskpri_irqs();

	IsolCpusMask = 0;

	printk(KERN_INFO "RTAI[hal]: mounted (%s, IMMEDIATE (INTERNAL IRQs VECTORED)).\n", HAL_TYPE);

	printk("PIPELINE layers:\n");
	for (trapnr = 1; ; trapnr++) {
		struct hal_domain_struct *next_domain;
		next_domain = get_domain_pointer(trapnr);
		if ((unsigned long)next_domain < 10) break;
		printk("%p %x %s %d\n", next_domain, next_domain->domid, next_domain->name, next_domain->priority);
	}


	return 0;
}

void __rtai_hal_exit (void)
{
	int trapnr;
#ifdef CONFIG_PROC_FS
	rtai_proc_unregister();
#endif
	hal_irq_handler = saved_hal_irq_handler;
	hal_unregister_domain(&rtai_domain);
	for (trapnr = 0; trapnr < HAL_NR_FAULTS; trapnr++) {
		hal_catch_event(hal_root_domain, trapnr, NULL);
	}
	hal_virtualize_irq(hal_root_domain, rtai_sysreq_virq, NULL, NULL, 0);
	hal_free_irq(rtai_sysreq_virq);
	rtai_uninstall_archdep();

	if (IsolCpusMask) {
		for (trapnr = 0; trapnr < IPIPE_NR_XIRQS; trapnr++) {
			rt_reset_irq_to_sym_mode(trapnr);
		}
	}

	printk(KERN_INFO "RTAI[hal]: unmounted.\n");
}

module_init(__rtai_hal_init);
module_exit(__rtai_hal_exit);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,60,0)
asmlinkage int rt_printk(const char *fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);
	r = vprintk(fmt, args);
	va_end(args);

	return r;
}

asmlinkage int rt_sync_printk(const char *fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);
	hal_set_printk_sync(&rtai_domain);
	r = vprintk(fmt, args);
	hal_set_printk_async(&rtai_domain);
	va_end(args);

	return r;
}
#else
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
	int r;

        va_start(args, fmt);
        vsnprintf(buf, VSNPRINTF_BUF, fmt, args);
        va_end(args);
	hal_set_printk_sync(&rtai_domain);
	r = printk("%s", buf);
	hal_set_printk_async(&rtai_domain);

	return r;
}
#endif

/*
 *  support for decoding long long numbers in kernel space.
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
	ul = ((unsigned long *)&ll)[1];
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
EXPORT_SYMBOL(rt_request_timer);
EXPORT_SYMBOL(rt_free_timer);
EXPORT_SYMBOL(rdtsc);
EXPORT_SYMBOL(rt_set_trap_handler);
EXPORT_SYMBOL(rt_set_ihook);
EXPORT_SYMBOL(rt_set_irq_ack);

EXPORT_SYMBOL(rtai_calibrate_8254);
EXPORT_SYMBOL(rtai_broadcast_to_local_timers);
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

/*@}*/

void (*rt_linux_hrt_set_mode)(int clock_event_mode, void *);
int (*rt_linux_hrt_next_shot)(unsigned long, void *);

//static int rtai_request_tickdev(void)   { return 0; }

//static void rtai_release_tickdev(void)  {  return;  }

EXPORT_SYMBOL(rt_linux_hrt_set_mode);
EXPORT_SYMBOL(rt_linux_hrt_next_shot);

void rt_release_rtc(void)
{
    return;
}

void rt_request_rtc(long rtc_freq, void *handler)
{
    return;
}

EXPORT_SYMBOL(rt_request_rtc);
EXPORT_SYMBOL(rt_release_rtc);
