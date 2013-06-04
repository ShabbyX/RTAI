/*
COPYRIGHT (C) 2000  Paolo Mantegazza (mantegazza@aero.polimi.it)
              2001  David Schleef <ds@schleef.org>
              2001  Lineo, Inc. <ds@lineo.com>
	      2002  Wolfgang Grandegger (wg@denx.de)

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
ACKNOWLEDGMENTS (LIKELY JUST A PRELIMINARY DRAFT):
- Steve Papacharalambous (stevep@zentropix.com) has contributed an informative
  proc filesystem procedure.
*/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#ifdef CONFIG_SMP
#include <linux/openpic.h>
#endif

#include <asm/system.h>
#include <asm/hw_irq.h>
#include <asm/smp.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/atomic.h>

#ifdef CONFIG_PROC_FS
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include "rtai_proc_fs.h"
#include "rtai_version.h"
#endif

#include <asm/rtai.h>
#include <asm/rtai_srq.h>

MODULE_LICENSE("GPL");

#ifndef CONFIG_RTAI_MOUNT_ON_LOAD
#define CONFIG_RTAI_MOUNT_ON_LOAD y
#endif

#ifdef CONFIG_MPC8260ADS
void mpc8260ads_set_led(int led,int state);
#endif

#include <rtai_trace.h>

// proc filesystem additions.
static int rtai_proc_register(void);
static void rtai_proc_unregister(void);
// End of proc filesystem additions.

/* Some defines */
#define BITS_PER_INT  	 	32

#ifdef CONFIG_SMP
/* The SMP code untested. We don't care about it for the moment */
#define NR_RTAI_IRQS  	 	(2 * BITS_PER_INT)
#define LAST_GLOBAL_RTAI_IRQ 	(BITS_PER_INT - 1)
#define HARD_LOCK_IPI 	        62
#else
#if NR_IRQS < BITS_PER_INT
#define NR_RTAI_IRQS  	 	NR_IRQS
#else
#define NR_RTAI_IRQS  	 	BITS_PER_INT
#endif
#define LAST_GLOBAL_RTAI_IRQ 	(NR_RTAI_IRQS - 1)
#endif

#define NR_SYSRQS  	 	32

#define TIMER_IRQ 	31

#define IRQ_DESC irq_desc

/*
 * This allows LINUX to handle IRQs requested with the function
 * rt_request_global_irq(). By default it's off for compatibility
 * with the i386 port.
 */
#undef GLOBAL_PEND_LINUX_IRQ

/* Debugging for events that change the interrupt flag
 *
 * If you want to use this, you need to define the functions
 * blink() or blink_out().  blink() is called with 0 or 1
 * to indicate whether interrupts are disabled or enabled.
 * blink_out() is called with the return address of the code
 * that triggered the interrupt flag change, with the low
 * bit set/unset as in blink.  (Since ppc code is always
 * word aligned, we can freely use the bottom 2 bits.)
 *
 * I use blink() to turn on/off LEDs and blink_out() to
 * write the return address serially over digital I/O lines
 * to a host system. -ds */

#if defined(DEBUG_INTR_BLINK)
#define set_intr_flag(a,b) do{			\
		(a)=(b);			\
		blink((b)&1);			\
	}while(0)
#define set_intr_flag_wasset(b) do{		\
		blink((b)&1);			\
	}while(0)
#elif defined(DEBUG_INTR_SERIAL)
#define set_intr_flag(a,b) do{			\
		(a)=(b);			\
		blink_out(((unsigned int)__builtin_return_address(0))|((b)&1));	\
	}while(0)
#define set_intr_flag_wasset(b) do{		\
		blink_out(((unsigned int)__builtin_return_address(0))|((b)&1));	\
	}while(0)
#else
#define set_intr_flag(a,b) do{ (a)=(b); }while(0)
#define set_intr_flag_wasset(b)
#endif

/* Most of our data */

#define RTAI_IRQ_UNMAPPED    0
#define RTAI_IRQ_MAPPED      1
#define RTAI_IRQ_MAPPED_TEMP 2

static struct irq_handling {
	int ppc_irq;
	int mapped;
#ifdef CONFIG_SMP
	volatile unsigned long dest_status;
#endif
	volatile int ext;
	unsigned long data;
	void (*handler)(unsigned int irq);
	unsigned int irq_count;
} global_irq[NR_RTAI_IRQS];

static int rtai_irq[NR_IRQS];
static struct hw_interrupt_type *linux_irq_desc_handler[NR_IRQS];

static int map_ppc_irq(int irq, int rtai_irq_from, int rtai_irq_to)
{
	int rirq;
	for (rirq =  rtai_irq_from; rirq <= rtai_irq_to; rirq++) {
		if (global_irq[rirq].mapped == RTAI_IRQ_UNMAPPED) {
			global_irq[rirq].mapped = RTAI_IRQ_MAPPED;
			global_irq[rirq].ppc_irq = irq;
			rtai_irq[irq] = rirq;
			return rirq;
		}
	}
	printk("map_ppc_irq: no more slot available for IRQ %d!\n", irq);
	return -1;
}

static inline int map_global_ppc_irq(int irq)
{
	return map_ppc_irq(irq, 0, LAST_GLOBAL_RTAI_IRQ);
}

#ifdef CONFIG_SMP
static inline void map_cpu_own_ppc_irq(int irq)
{
	return map_ppc_irq(irq, LAST_GLOBAL_RTAI_IRQ + 1, NR_RTAI_IRQS - 1);
}
#endif

static inline void unmap_ppc_irq(int irq)
{
	int rirq;
	if ((rirq = rtai_irq[irq]) >= 0) {
		if (global_irq[rirq].mapped == RTAI_IRQ_MAPPED_TEMP) {
			global_irq[rirq].mapped = RTAI_IRQ_UNMAPPED;
			global_irq[rirq].ppc_irq = 0;
			rtai_irq[irq] = -1;
		} else {
			printk("unmap_ppc_irq: oops, conistency error!\n");
		}
	} else {
		printk("unmap_ppc_irq: IRQ %d is not mapped!\n", irq);
	}
}

static struct sysrq_t {
	unsigned int label;
	void (*rtai_handler)(void);
	long long (*user_handler)(unsigned int whatever);
} sysrq[NR_SYSRQS];

// The main items to be saved-restored to make Linux our humble slave

extern void ppc_irq_dispatch_handler(struct pt_regs *regs, int irq);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,3)
struct int_control_struct ppc_int_control;
#endif
static int (*ppc_get_irq)(struct pt_regs *regs);
unsigned long ppc_irq_dispatcher, ppc_timer_handler;
extern int (*rtai_srq_bckdr)(struct pt_regs *regs);

/* Hook in the interrupt return path */
extern void (*rtai_soft_sti)(void);

static struct pt_regs rtai_regs;  // Dummy registers.

static struct global_rt_status {
	volatile unsigned int pending_irqs;
	volatile unsigned int activ_irqs;
	volatile unsigned int pending_srqs;
	volatile unsigned int activ_srqs;
	volatile unsigned int cpu_in_sti;
	volatile unsigned int used_by_linux;
  	volatile unsigned int locked_cpus;
#ifdef CONFIG_SMP
  	volatile unsigned int hard_nesting;
	volatile unsigned int hard_lock_all_service;
	spinlock_t hard_lock;
#endif
	spinlock_t data_lock;
	spinlock_t ic_lock;
} global;

volatile unsigned int *locked_cpus = &global.locked_cpus;

static struct cpu_own_status {
	volatile unsigned int intr_flag;
	volatile unsigned int linux_intr_flag;
	volatile unsigned int pending_irqs;
	volatile unsigned int activ_irqs;
	void (*rt_timer_handler)(void);
	void (*trailing_irq_handler)(int irq, void *dev_id, struct pt_regs *regs);
} processor[NR_RT_CPUS];

#ifdef CONFIG_SMP

/* Our interprocessor messaging */

void send_ipi_shorthand(unsigned int shorthand, int irq)
{
	int cpuid;
	unsigned long flags;
	cpuid = hard_cpu_id();
	hard_save_flags(flags);
	hard_cli();
	if (shorthand == APIC_DEST_ALLINC) {
		openpic_cause_IPI(cpuid, irq, 0xFFFFFFFF);
	} else {
		openpic_cause_IPI(cpuid, irq, 0xFFFFFFFF & ~(1 << cpuid));
	}
	hard_restore_flags(flags);
}

void send_ipi_logical(unsigned long dest, int irq)
{
	unsigned long flags;
	if ((dest &= cpu_online_map)) {
		hard_save_flags(flags);
		openpic_cause_IPI(hard_cpu_id(), irq, dest);
		hard_cli();
		hard_restore_flags(flags);
	}
}

static void hard_lock_all_handler(void)
{
	int cpuid;
	set_bit(cpuid = hard_cpu_id(), &global_irq[HARD_LOCK_IPI].dest_status);
	rt_spin_lock(&(global.hard_lock));
// No service at the moment.
	rt_spin_unlock(&(global.hard_lock));
	clear_bit(cpuid, &globa_irql[HARD_LOCK_IPI].dest_status);
}

static inline int hard_lock_all(void)
{
	unsigned long flags;
	flags = rt_global_save_flags_and_cli();
	if (!global.hard_nesting++) {
		global.hard_lock_all_service = 0;
		rt_spin_lock(&(global.hard_lock));
		send_ipi_shorthand(APIC_DEST_ALLBUT, HARD_LOCK_IPI);
		while (global_irq[HARD_LOCK_IPI].dest_status != (cpu_online_map & ~global.locked_cpus));
	}
	return flags;
}

static inline void hard_unlock_all(unsigned long flags)
{
	if (global.hard_nesting > 0) {
		if (!(--global.hard_nesting)) {
			rt_spin_unlock(&(global.hard_lock));
			while (global_irq[HARD_LOCK_IPI].dest_status);
		}
	}
	rt_global_restore_flags(flags);
}

#else

void send_ipi_shorthand(unsigned int shorthand, int irq) { }

void send_ipi_logical(unsigned long dest, int irq) { }

#if 0
static void hard_lock_all_handler(void) { }
#endif

static inline unsigned long hard_lock_all(void)
{
	unsigned long flags;
	hard_save_flags_and_cli(flags);
	return flags;
}

#define hard_unlock_all(flags) hard_restore_flags((flags))

#endif

static void linux_cli(void)
{
	set_intr_flag(processor[hard_cpu_id()].intr_flag,0);
}

#if 0
static void linux_soft_sti(void)
{
       	unsigned long cpuid;
       	struct cpu_own_status *cpu;

	cpu = processor + (cpuid = hard_cpu_id());
	set_intr_flag(cpu->intr_flag,(1 << IFLAG) | (1 << cpuid));
}
#endif

static void run_pending_irqs(void)
{
       	unsigned long irq, cpuid;
       	struct cpu_own_status *cpu;

	cpuid = hard_cpu_id();
	if (!test_and_set_bit(cpuid, &global.cpu_in_sti)) {

		cpu = processor + cpuid;
		while (global.pending_irqs | cpu->pending_irqs | global.pending_srqs) {
			hard_cli();
			if ((irq = cpu->pending_irqs & ~(cpu->activ_irqs))) {
				irq = ffnz(irq);
				set_bit(irq, &cpu->activ_irqs);
				clear_bit(irq, &cpu->pending_irqs);
				hard_sti();
				set_intr_flag(cpu->intr_flag,0);
				if (irq == TIMER_IRQ) {
					timer_interrupt(&rtai_regs);
					if (cpu->trailing_irq_handler) {
						(cpu->trailing_irq_handler)(TIMER_IRQ, 0, &rtai_regs);
					}
		       		} else {
					printk("WHY HERE? AT THE MOMENT IT IS JUST UP, SO NO LOCAL IRQs.\n");
				}
				clear_bit(irq, &cpu->activ_irqs);
	       		} else {
				hard_sti();
			}

			rt_spin_lock_irq(&(global.data_lock));
			if ((irq = global.pending_srqs & ~global.activ_srqs)) {
				irq = ffnz(irq);
				set_bit(irq, &global.activ_srqs);
				clear_bit(irq, &global.pending_srqs);
				rt_spin_unlock_irq(&(global.data_lock));
				if (sysrq[irq].rtai_handler) {
					sysrq[irq].rtai_handler();
				}
				clear_bit(irq, &global.activ_srqs);
			} else {
				rt_spin_unlock_irq(&(global.data_lock));
			}

			rt_spin_lock_irq(&(global.data_lock));
			if ((irq = global.pending_irqs & ~global.activ_irqs)) {
				irq = ffnz(irq);
				set_bit(irq, &global.activ_irqs);
				clear_bit(irq, &global.pending_irqs);
				rt_spin_unlock_irq(&(global.data_lock));
				set_intr_flag(cpu->intr_flag,0);
                                /* from Linux do_IRQ */
                                hardirq_enter(cpuid);
				ppc_irq_dispatch_handler(&rtai_regs, global_irq[irq].ppc_irq);
                                hardirq_exit(cpuid);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10) /* ? */
				if (softirq_pending(cpuid)) {
					do_softirq();
				}
#endif
				clear_bit(irq, &global.activ_irqs);
			} else {
				rt_spin_unlock_irq(&(global.data_lock));
			}
		}
		clear_bit(cpuid, &global.cpu_in_sti);
	}
	/* We _should_ do this, but it doesn't work correctly */
	//if(atomic_read(&ppc_n_lost_interrupts))do_lost_interrupts(MSR_EE);
}

static void linux_sti(void)
{
	run_pending_irqs();
	set_intr_flag(processor[hard_cpu_id()].intr_flag,
		      (1 << IFLAG) | (1 << hard_cpu_id()));
}

static void linux_save_flags(unsigned long *flags)
{
	*flags = processor[hard_cpu_id()].intr_flag;
}

static void linux_restore_flags(unsigned long flags)
{
	if (flags) {
		linux_sti();
	} else {
		set_intr_flag(processor[hard_cpu_id()].intr_flag,0);
	}
}

unsigned int linux_save_flags_and_cli(void)
{
	unsigned int ret;

	ret = xchg_u32((void *)(&(processor[hard_cpu_id()].intr_flag)), 0);
	set_intr_flag_wasset(0);
	return ret;
}

unsigned int linux_save_flags_and_cli_cpuid(int cpuid)  // LXRT specific
{
	return xchg_u32((void *)&(processor[cpuid].intr_flag), 0);
}

void rtai_just_copy_back(unsigned long flags, int cpuid)
{
        set_intr_flag(processor[cpuid].intr_flag,flags);
}

static void (*ic_ack_irq[NR_IRQS])    (unsigned int irq);

static void do_nothing_picfun(unsigned int irq) { };

unsigned int rt_startup_irq(unsigned int irq)
{
	unsigned long flags, retval;
	struct hw_interrupt_type *irq_desc;

	if ((irq_desc = linux_irq_desc_handler[irq]) && irq_desc->startup) {
		flags = rt_spin_lock_irqsave(&global.ic_lock);
		retval = irq_desc->startup(irq);
		rt_spin_unlock_irqrestore(flags, &global.ic_lock);
		return retval;
	}
	return 0;
}

void rt_shutdown_irq(unsigned int irq)
{
	unsigned int flags;
	struct hw_interrupt_type *irq_desc;

	if ((irq_desc = linux_irq_desc_handler[irq]) && irq_desc->shutdown) {
		flags = rt_spin_lock_irqsave(&global.ic_lock);
		irq_desc->shutdown(irq);
		rt_spin_unlock_irqrestore(flags, &global.ic_lock);
	}
}

void rt_enable_irq(unsigned int irq)
{
	unsigned int flags;
	struct hw_interrupt_type *irq_desc;

	if ((irq_desc = linux_irq_desc_handler[irq]) && irq_desc->enable) {
		flags = rt_spin_lock_irqsave(&global.ic_lock);
		irq_desc->enable(irq);
		rt_spin_unlock_irqrestore(flags, &global.ic_lock);
	}
}

void rt_disable_irq(unsigned int irq)
{
	unsigned int flags;
	struct hw_interrupt_type *irq_desc;

	if ((irq_desc = linux_irq_desc_handler[irq]) && irq_desc->disable) {
		flags = rt_spin_lock_irqsave(&global.ic_lock);
		irq_desc->disable(irq);
		rt_spin_unlock_irqrestore(flags, &global.ic_lock);
	}
}

void rt_ack_irq(unsigned int irq)
{
	unsigned int flags;

	flags = rt_spin_lock_irqsave(&global.ic_lock);
	if(ic_ack_irq[irq])
		ic_ack_irq[irq](irq);
	rt_spin_unlock_irqrestore(flags, &global.ic_lock);
}

void rt_unmask_irq(unsigned int irq)
{
	unsigned int flags;
	struct hw_interrupt_type *irq_desc;

	if ((irq_desc = linux_irq_desc_handler[irq])) {

		flags = rt_spin_lock_irqsave(&global.ic_lock);
		if(linux_irq_desc_handler[irq]->end)
			linux_irq_desc_handler[irq]->end(irq);
		else if(linux_irq_desc_handler[irq]->enable)
			linux_irq_desc_handler[irq]->enable(irq);
		rt_spin_unlock_irqrestore(flags, &global.ic_lock);
	}
}

static int dispatch_irq(struct pt_regs *regs, int isfake)
{
	int irq, rirq;

	rt_spin_lock(&global.ic_lock);
	if ((irq = ppc_get_irq(regs)) >= 0) {
		if(ic_ack_irq[irq])
			ic_ack_irq[irq](irq);            // Any umasking must be done in the irq handler.
		rt_spin_unlock(&global.ic_lock);

		TRACE_RTAI_GLOBAL_IRQ_ENTRY(irq, !user_mode(regs));

		if ((rirq = rtai_irq[irq]) < 0) {
			rirq = map_global_ppc_irq(irq); /* not yet mapped */
		}

		if (global_irq[rirq].handler) {
			global_irq[rirq].irq_count++;
			if (global_irq[rirq].ext) {
				if (((int (*)(int, unsigned long))global_irq[rirq].handler)(irq, global_irq[rirq].data)) {
					return 0;
				}
			} else {
				((void (*)(int))global_irq[rirq].handler)(irq);
			}
			rt_spin_lock(&(global.data_lock));
		} else {
			rt_spin_lock(&(global.data_lock));
#ifdef CONFIG_SMP
			if (rirq => LAST_GLOBAL_RTAI_IRQ) {
				set_bit(rirq, &processor[hard_cpu_id()].pending_irqs);
			} else {
				set_bit(rirq, &(global.pending_irqs));
			}
#else
			set_bit(rirq, &(global.pending_irqs));
#endif
		}
		if ((global.used_by_linux & processor[hard_cpu_id()].intr_flag)) {
			rt_spin_unlock_irq(&(global.data_lock));
			run_pending_irqs();

			TRACE_RTAI_GLOBAL_IRQ_EXIT();

			return 1;
		} else {
			rt_spin_unlock(&(global.data_lock));

			TRACE_RTAI_GLOBAL_IRQ_EXIT();

			return 0;
		}

		TRACE_RTAI_GLOBAL_IRQ_EXIT(); /* PARANOIA */

	} else {
		rt_spin_unlock(&global.ic_lock);
		return 0;

	}
}

static int dispatch_timer_irq(struct pt_regs *regs)
{
	int cpuid;

	/*
	 * TRACE_RTAI_TRAP_ENTRY is not yet handled correctly by LTT.
	 * Therefore we treat the decrementer trap like an IRQ 255
	 */
	TRACE_RTAI_GLOBAL_IRQ_ENTRY(255, !user_mode(regs));

	if (processor[cpuid = hard_cpu_id()].rt_timer_handler) {
		(processor[cpuid].rt_timer_handler)();
		rt_spin_lock(&(global.data_lock));
	} else {
		rt_spin_lock(&(global.data_lock));
		set_bit(TIMER_IRQ, &processor[cpuid].pending_irqs);
	}
	if ((global.used_by_linux & processor[cpuid].intr_flag)) {
		rt_spin_unlock_irq(&(global.data_lock));
		run_pending_irqs();
		TRACE_RTAI_GLOBAL_IRQ_EXIT();
		return 1;
	} else {
		rt_spin_unlock(&(global.data_lock));
		TRACE_RTAI_GLOBAL_IRQ_EXIT();
		return 0;
	}
}

#define MIN_IDT_VEC 0xF0
#define MAX_IDT_VEC 0xFF

static unsigned long long (*idt_table[MAX_IDT_VEC - MIN_IDT_VEC + 1])(int srq, unsigned long name);

static int dispatch_srq(struct pt_regs *regs)
{
	unsigned long vec, srq, whatever;
	long long retval;

	if (regs->gpr[0] && regs->gpr[0] == ((srq = regs->gpr[3]) + (whatever = regs->gpr[4]))) {

	        TRACE_RTAI_SRQ_ENTRY(srq, !user_mode(regs));

		if (!(vec = srq >> 24)) {
			if (srq > 1 && srq < NR_SYSRQS && sysrq[srq].user_handler) {
				retval = sysrq[srq].user_handler(whatever);
			} else {
				for (srq = 2; srq < NR_SYSRQS; srq++) {
					if (sysrq[srq].label == whatever) {
						retval = srq;
					}
				}
			}
		} else {
			retval = idt_table[vec - MIN_IDT_VEC](srq & 0xFFFFFF, whatever);
		}
		regs->gpr[0] = 0;
		regs->gpr[3] = ((unsigned long *)&retval)[0];
		regs->gpr[4] = ((unsigned long *)&retval)[1];
		regs->nip += 4;

		TRACE_RTAI_SRQ_EXIT();

		return 0;
	} else {
		return 1;
	}
}

struct desc_struct rt_set_full_intr_vect(unsigned int vector, int type, int dpl, void *handler)
{
	struct desc_struct fun = { 0 };
	if (vector >= MIN_IDT_VEC && vector <= MAX_IDT_VEC) {
		fun.fun = idt_table[vector - MIN_IDT_VEC];
		idt_table[vector - MIN_IDT_VEC] = handler;
		if (!rtai_srq_bckdr) {
			rtai_srq_bckdr = dispatch_srq;
		}
	}
	return fun;
}

void rt_reset_full_intr_vect(unsigned int vector, struct desc_struct idt_element)
{
	if (vector >= MIN_IDT_VEC && vector <= MAX_IDT_VEC) {
		idt_table[vector - MIN_IDT_VEC] = idt_element.fun;
	}
}

/* Here are the trapped irq actions for Linux interrupt handlers. */

static void trpd_enable_irq(unsigned int irq)
{
	rt_spin_lock_irq(&global.ic_lock);
	if(linux_irq_desc_handler[irq]->enable)
		linux_irq_desc_handler[irq]->enable(irq);
	rt_spin_unlock_irq(&global.ic_lock);
}

static void trpd_disable_irq(unsigned int irq)
{
	rt_spin_lock_irq(&global.ic_lock);
	if(linux_irq_desc_handler[irq]->disable)
		linux_irq_desc_handler[irq]->disable(irq);
	rt_spin_unlock_irq(&global.ic_lock);
}

static int trpd_get_irq(struct pt_regs *regs)
{
	printk("TRAPPED GET_IRQ SHOULD NEVER BE CALLED\n");
	return -1;
}

static void trpd_ack_irq(unsigned int irq)
{
#if 0
        rt_spin_lock_irq(&global.ic_lock);
	if(linux_irq_desc_handler[irq]->ack)
		linux_irq_desc_handler[irq]->ack(irq);
	rt_spin_unlock_irq(&global.ic_lock);
#endif
}

static void trpd_end_irq(unsigned int irq)
{
        rt_spin_lock_irq(&global.ic_lock);
	if(linux_irq_desc_handler[irq]->end)
		linux_irq_desc_handler[irq]->end(irq);
	else if(linux_irq_desc_handler[irq]->enable)
		linux_irq_desc_handler[irq]->enable(irq);
	rt_spin_unlock_irq(&global.ic_lock);
}

static struct hw_interrupt_type trapped_linux_irq_type = {
	typename:	"RT SPVISD",
	startup:	rt_startup_irq,
	shutdown:	rt_shutdown_irq,
	enable:		trpd_enable_irq,
	disable:	trpd_disable_irq,
	ack:		trpd_ack_irq,
	end:		trpd_end_irq,
	set_affinity:	NULL,
};

#ifndef GLOBAL_PEND_LINUX_IRQ
static struct hw_interrupt_type real_time_irq_type = {
	typename:	"REAL TIME",
	startup:	(unsigned int (*)(unsigned int))do_nothing_picfun,
	shutdown:	do_nothing_picfun,
	enable:		do_nothing_picfun,
	disable:	do_nothing_picfun,
	ack:		do_nothing_picfun,
	end:		do_nothing_picfun,
	set_affinity:	NULL,
};
#endif

/* Request and free interrupts, system requests and interprocessors messages */
/* Request for regular Linux irqs also included. They are nicely chained to  */
/* Linux, forcing sharing with any already installed handler, so that we can */
/* have an echo from Linux for global handlers. We found that usefull during */
/* debug, but can be nice for a lot of other things, e.g. see the jiffies    */
/* recovery in rtai_sched.c, and the global broadcast to local apic timers.  */

static unsigned long irq_action_flags[NR_IRQS];
static int chained_to_linux[NR_IRQS];

int rt_request_global_irq(unsigned int irq, void (*handler)(unsigned int irq))
{
	unsigned long flags;
	int rirq;

	if (irq >= NR_IRQS || !handler) {
		return -EINVAL;
	}
	rirq = rtai_irq[irq];
	if (rirq >= 0 && global_irq[rirq].handler) {
		return -EBUSY;
	}

	flags = hard_lock_all();
	if (rirq < 0) {
#ifdef CONFIG_SMP
		if (irq < OPENPIC_VEC_IPI) {
			rirq = map_global_ppc_irq(irq);
		} else {
			rirq = map_cpu_own_ppc_irq(irq);
		}
		global_irq[rirq].dest_status = 0;
#else
		rirq = map_global_ppc_irq(irq);
#endif
		global_irq[rirq].mapped = RTAI_IRQ_MAPPED_TEMP;
	}
	global_irq[rirq].handler = handler;
#ifndef GLOBAL_PEND_LINUX_IRQ
	/*
	 * If you use rt_pend_linux_irq(), this might not be what
	 * you expect or want especially with level sensitive IRQ.
	 * We keep this part mainly for compatibility with i386.
	 */
	IRQ_DESC[irq].handler = &real_time_irq_type;
#endif
	hard_unlock_all(flags);

	return 0;
}

int rt_request_global_irq_ext(unsigned int irq,
			      int (*handler)(unsigned int irq, unsigned long data),
			      unsigned long data)
{
	int rirq, ret;
	if (!(ret = rt_request_global_irq(irq, (void (*)(unsigned int irq))handler))) {
		rirq = rtai_irq[irq];
		global_irq[rirq].ext = 1;
		global_irq[rirq].data = data;
		return 0;
	}
	return ret;
}

void rt_set_global_irq_ext(unsigned int irq, int ext, unsigned long data)
{
	int rirq;
	if (irq < NR_IRQS && (rirq = rtai_irq[irq]) >= 0) {
		global_irq[rirq].ext = ext ? 1 : 0;
		global_irq[rirq].data = data;
	} else {
		printk("rt_set_global_irq_ext: irq=%d is invalid\n", irq);
	}
}

int rt_free_global_irq(unsigned int irq)
{
	unsigned long flags;
	int rirq;

	if (irq < 0 || irq >= NR_IRQS) {
		return -EINVAL;
	}
	rirq = rtai_irq[irq];
	if (rirq < 0 || !global_irq[rirq].handler) {
		return -EINVAL;
	}

	flags = hard_lock_all();
	IRQ_DESC[irq].handler = &trapped_linux_irq_type;
	if (global_irq[rirq].mapped == RTAI_IRQ_MAPPED_TEMP)
		unmap_ppc_irq(irq);
#ifdef CONFIG_SMP
	global_irq[rirq].dest_status = 0;
#endif
	global_irq[rirq].handler = 0;
	global_irq[rirq].ext = 0;
	global_irq[rirq].irq_count = 0;
	hard_unlock_all(flags);

	return 0;
}

int rt_request_linux_irq(unsigned int irq,
	void (*linux_handler)(int irq, void *dev_id, struct pt_regs *regs),
	char *linux_handler_id, void *dev_id)
{
	unsigned long flags;

	if (irq == TIMER_8254_IRQ) {
		processor[0].trailing_irq_handler = linux_handler;
		return 0;
	}

	if (irq >= NR_IRQS || !linux_handler) {
		return -EINVAL;
	}

	save_flags(flags);
	cli();
	if (!chained_to_linux[irq]++) {
		if (IRQ_DESC[irq].action) {
			irq_action_flags[irq] = IRQ_DESC[irq].action->flags;
			IRQ_DESC[irq].action->flags |= SA_SHIRQ;
		}
	}
	restore_flags(flags);
#if defined(CONFIG_8xx) || defined(CONFIG_8260)
	request_8xxirq(irq, linux_handler, SA_SHIRQ, linux_handler_id, dev_id);
#else
	request_irq(irq, linux_handler, SA_SHIRQ, linux_handler_id, dev_id);
#endif
	return 0;
}

int rt_free_linux_irq(unsigned int irq, void *dev_id)
{
	unsigned long flags;

	if (irq == TIMER_8254_IRQ) {
		processor[0].trailing_irq_handler = 0;
		return 0;
	}

	if (irq >= NR_IRQS || !chained_to_linux[irq]) {
		return -EINVAL;
	}

	free_irq(irq, dev_id);
	save_flags(flags);
	cli();
	if (!(--chained_to_linux[irq]) && IRQ_DESC[irq].action) {
		IRQ_DESC[irq].action->flags = irq_action_flags[irq];
	}
	restore_flags(flags);

	return 0;
}

void rt_pend_linux_irq(unsigned int irq)
{
	if (irq == TIMER_8254_IRQ) {
		set_bit(TIMER_IRQ, &processor[hard_cpu_id()].pending_irqs);
		return;
	}
	if ((irq = rtai_irq[irq]) <= LAST_GLOBAL_RTAI_IRQ) {
		set_bit(irq, &global.pending_irqs);
	}
}

void rt_tick_linux_timer(void)
{
	set_bit(TIMER_IRQ, &processor[hard_cpu_id()].pending_irqs);
}

int rt_request_srq(unsigned int label, void (*rtai_handler)(void), long long (*user_handler)(unsigned int whatever))
{
	unsigned long flags;
	int srq;

	flags = rt_spin_lock_irqsave(&global.data_lock);
	if (!rtai_handler) {
		rt_spin_unlock_irqrestore(flags, &global.data_lock);
		return -EINVAL;
	}
	for (srq = 2; srq < NR_SYSRQS; srq++) {
		if (!(sysrq[srq].rtai_handler)) {
			sysrq[srq].rtai_handler = rtai_handler;
			sysrq[srq].label = label;
			if (user_handler) {
				sysrq[srq].user_handler = user_handler;
				if (!rtai_srq_bckdr) {
					rtai_srq_bckdr = dispatch_srq;
				}
			}
			rt_spin_unlock_irqrestore(flags, &global.data_lock);
			return srq;
		}
	}
	rt_spin_unlock_irqrestore(flags, &global.data_lock);

	return -EBUSY;
}

int rt_free_srq(unsigned int srq)
{
	unsigned long flags;

	flags = rt_spin_lock_irqsave(&global.data_lock);
	if (srq < 2 || srq >= NR_SYSRQS || !sysrq[srq].rtai_handler) {
		rt_spin_unlock_irqrestore(flags, &global.data_lock);
		return -EINVAL;
	}
	sysrq[srq].rtai_handler = 0;
	sysrq[srq].user_handler = 0;
	sysrq[srq].label = 0;
	for (srq = 2; srq < NR_SYSRQS; srq++) {
		if (sysrq[srq].user_handler) {
			rt_spin_unlock_irqrestore(flags, &global.data_lock);
			return 0;
		}
	}
	if (rtai_srq_bckdr) {
		rtai_srq_bckdr = 0;
	}
	rt_spin_unlock_irqrestore(flags, &global.data_lock);

	return 0;
}

void rt_pend_linux_srq(unsigned int srq)
{
	set_bit(srq, &global.pending_srqs);
}

void rt_switch_to_linux(int cpuid)
{

	TRACE_RTAI_SWITCHTO_LINUX(cpuid);

	set_bit(cpuid, &global.used_by_linux);
	set_intr_flag(processor[cpuid].intr_flag,processor[cpuid].linux_intr_flag);
}

void rt_switch_to_real_time(int cpuid)
{

	TRACE_RTAI_SWITCHTO_RT(cpuid);

	processor[cpuid].linux_intr_flag = processor[cpuid].intr_flag;
	set_intr_flag(processor[cpuid].intr_flag,0);
	clear_bit(cpuid, &global.used_by_linux);
}

/* RTAI mount-unmount functions to be called from the application to       */
/* initialise the real time application interface, i.e. this module, only  */
/* when it is required; so that it can stay asleep when it is not needed   */

#ifdef CONFIG_RTAI_MOUNT_ON_LOAD
#define rtai_mounted 1
#else
static spinlock_t rtai_mount_lock = SPIN_LOCK_UNLOCKED;
static int rtai_mounted;
#endif

// Trivial, but we do things carefully, the blocking part is relatively short,
// should cause no troubles in the transition phase.
// All the zeroings are strictly not required as mostly related to static data.
// Done esplicitly for emphasis. Simple, just block other processors and grab
// everything from Linux.  To this aim first block all the other cpus by using
// a dedicated HARD_LOCK_IPI and its vector without any protection.

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,2)
/*
 * This is the binary patch we're applying to the Linux kernel.
 *
00000000 <patch>:
   0:	94 21 ff f0 	stwu	r1,-16(r1)
   4:	7c 08 02 a6 	mflr	r0
   8:	90 01 00 14 	stw	r0,20(r1)
   c:	3c 00 00 00 	lis	r0,0
			e: R_PPC_ADDR16_HI	func
  10:	60 00 00 00 	nop
			12: R_PPC_ADDR16_LO	func
  14:	7c 08 03 a6 	mtlr	r0
  18:	4e 80 00 21 	blrl
  1c:	80 01 00 14 	lwz	r0,20(r1)
  20:	7c 08 03 a6 	mtlr	r0
  24:	38 21 00 10 	addi	r1,r1,16
  28:	4e 80 00 20 	blr
*/

#if 0
/* This function creates essentially the same code as the above
 * fragment, if you want to know where it comes from. */
void patch_prototype(void)
{
	(*(void (*)(void))0xc001dafe)();
}
#endif

static u32 patch_insns[]={
	0x9421fff0, 0x7c0802a6, 0x90010014, 0x3c000000,
	0x60000000, 0x7c0803a6, 0x4e800021, 0x80010014,
	0x7c0803a6, 0x38210010, 0x4e800020,
};
#define N_INSNS 11
static u32 saved_insns_cli[N_INSNS];
static u32 saved_insns_sti[N_INSNS];
static u32 saved_insns_save_flags[N_INSNS];
static u32 saved_insns_restore_flags[N_INSNS];


static void patch_function(void *dest,void *func,void *save)
{
	memcpy(save,dest,sizeof(patch_insns));
	memcpy(dest,patch_insns,sizeof(patch_insns));
	((u32 *)dest)[3]|=(((unsigned int)(func))>>16)&0xffff;
	((u32 *)dest)[4]|=((unsigned int)(func))&0xffff;
	flush_icache_range((int)dest,(int)dest+sizeof(patch_insns));
}

static void unpatch_function(void *dest,void *save)
{
	memcpy(dest,save,sizeof(patch_insns));
	flush_icache_range((int)dest,(int)dest+sizeof(patch_insns));
}

static void install_patch(void)
{
	patch_function(&__cli,&linux_cli,saved_insns_cli);
	patch_function(&__sti,&linux_sti,saved_insns_sti);
	patch_function(&__save_flags_ptr,&linux_save_flags,saved_insns_save_flags);
	patch_function(&__restore_flags,&linux_restore_flags,saved_insns_restore_flags);
}

static void uninstall_patch(void)
{
	unpatch_function(&__cli,saved_insns_cli);
	unpatch_function(&__sti,saved_insns_sti);
	unpatch_function(&__save_flags_ptr,saved_insns_save_flags);
	unpatch_function(&__restore_flags,saved_insns_restore_flags);
}
#endif

void __rt_mount_rtai(void)
{
#define MSR(x) ((struct pt_regs *)((x)->thread.ksp + STACK_FRAME_OVERHEAD))->msr
	struct task_struct *task;
     	unsigned long flags, i;

	printk("rtai: mounting\n");
#ifdef CONFIG_SMP
	global_irq[HARD_LOCK_IPI].handler = hard_lock_all_handler;
#endif
	flags = hard_lock_all();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,2) // approximate
	install_patch();
#else
	int_control.int_cli           = linux_cli;
	int_control.int_sti           = linux_sti;
	int_control.int_save_flags    = linux_save_flags;
	int_control.int_restore_flags = linux_restore_flags;
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,3) // approximate
	int_control.int_set_lost      = (void (*)(unsigned long))rt_pend_linux_irq;
#endif
	ppc_md.get_irq = trpd_get_irq;
	do_IRQ_intercept = (unsigned long)dispatch_irq;
	timer_interrupt_intercept = (unsigned long)dispatch_timer_irq;
	rtai_soft_sti = linux_sti; /* and not linux_soft_sti */
	atomic_set(&ppc_n_lost_interrupts, 0);
	for (i = 0; i < NR_MASK_WORDS; i++) {
		ppc_lost_interrupts[i] = 0;
	}

	for (i = 0; i < NR_IRQS; i++) {
		if (IRQ_DESC[i].handler) {
			IRQ_DESC[i].handler = &trapped_linux_irq_type;
		}
	}

	task = &init_task;
	MSR(task) |= MSR_EE;
	if (task->thread.regs) {
		(task->thread.regs)->msr |= MSR_EE;
	}
	for_each_task(task) {
		MSR(task) |= MSR_EE;
		if (task->thread.regs) {
			(task->thread.regs)->msr |= MSR_EE;
		}
	}

	hard_unlock_all(flags);
	//printk("\n***** RTAI NEWLY MOUNTED (MOUNT COUNT %d) ******\n\n", rtai_mounted);
	printk("rtai: mount done\n");
}

// Simple, now we can simply block other processors and copy original data back
// to Linux. The HARD_LOCK_IPI is the last one to be reset.
void __rt_umount_rtai(void)
{
	int i;
	unsigned long flags;

	flags = hard_lock_all();
	rtai_srq_bckdr			= 0;
	rtai_soft_sti			= 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,2)
	uninstall_patch();
	for(i=0;i<NR_CPUS;i++){
		disarm_decr[i]=0;
	}
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,3)
	int_control               = ppc_int_control;
#endif
	timer_interrupt_intercept = ppc_timer_handler;
	do_IRQ_intercept          = ppc_irq_dispatcher;
	ppc_md.get_irq            = ppc_get_irq;
	for (i = 0; i < NR_IRQS; i++) {
		IRQ_DESC[i].handler = linux_irq_desc_handler[i];
	}
#ifdef CONFIG_SMP
	global_irq[HARD_LOCK_IPI].handler = 0;
	global_irq[HARD_LOCK_IPI].dest_status = 0;
#endif
#ifdef CONFIG_4xx
        /* Reenable auto-reload mode used by Linux */
        if ((mfspr(SPRN_TCR) & TCR_ARE) == 0) {
		while (get_dec_4xx() > 0);
		mtspr(SPRN_TCR,  mfspr(SPRN_TCR) | TCR_ARE);
		set_dec_4xx(tb_ticks_per_jiffy);
        }
#else
	while (get_dec() <= tb_ticks_per_jiffy);
	set_dec(tb_ticks_per_jiffy);
#endif
	/* XXX We probably should change the per-process soft
	 * interrupt flags back to MSR_EE here. */
	hard_unlock_all(flags);
	printk("\n***** RTAI UNMOUNTED (MOUNT COUNT %d) ******\n\n", rtai_mounted);
}

#ifdef CONFIG_RTAI_MOUNT_ON_LOAD
void rt_mount_rtai(void) { }
void rt_umount_rtai(void) { }
#else
void rt_mount_rtai(void)
{
	rt_spin_lock(&rtai_mount_lock);
	rtai_mounted++;
	MOD_INC_USE_COUNT;

	TRACE_RTAI_MOUNT();

	if(rtai_mounted==1)__rtai_mount_rtai();
	rt_spin_unlock(&rtai_mount_lock);
}

void rt_umount_rtai(void)
{
	rt_spin_lock(&rtai_mount_lock);
	rtai_mounted--;
	MOD_DEC_USE_COUNT;

	TRACE_RTAI_UMOUNT();

	if(!rtai_mounted)__rtai_umount_rtai();
	rt_spin_unlock(&rtai_mount_lock);

}
#endif

// Module parameters to allow frequencies to be overriden via insmod
static int CpuFreq = 0;
MODULE_PARM(CpuFreq, "i");

/* module init-cleanup */

static void rt_printk_sysreq_handler(void);

// Let's prepare our side without any problem, so that there remain just a few
// things to be done when mounting RTAI. All the zeroings are strictly not
// required as mostly related to static data. Done esplicitly for emphasis.
int init_module(void)
{
     	unsigned int i;

	// Passed in CPU frequency overides auto detected Linux value
	if (CpuFreq == 0) {
	    	extern unsigned tb_ticks_per_jiffy;

		CpuFreq = HZ * tb_ticks_per_jiffy;
	}
	tuned.cpu_freq = CpuFreq;
	printk("rtai: decrementer frequency %d Hz\n",tuned.cpu_freq);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,3)
	ppc_int_control    = int_control;
#endif
	ppc_get_irq        = ppc_md.get_irq;
	ppc_irq_dispatcher = do_IRQ_intercept;
	ppc_timer_handler  = timer_interrupt_intercept;

       	global.pending_irqs    = 0;
       	global.activ_irqs      = 0;
       	global.pending_srqs    = 0;
       	global.activ_srqs      = 0;
       	global.cpu_in_sti      = 0;
	global.used_by_linux   = ~(0xFFFFFFFF << smp_num_cpus);
#ifdef CONFIG_SMP
	global.locked_cpus     = 0;
	global.hard_nesting    = 0;
      	spin_lock_init(&(global.hard_lock));
#endif
      	spin_lock_init(&(global.data_lock));
      	spin_lock_init(&global.global.ic_lock);

	for (i = 0; i < NR_RT_CPUS; i++) {
		processor[i].intr_flag         = (1 << IFLAG) | (1 << i);
		processor[i].linux_intr_flag   = (1 << IFLAG) | (1 << i);
		processor[i].pending_irqs      = 0;
		processor[i].activ_irqs        = 0;
		processor[i].rt_timer_handler  = 0;
		processor[i].trailing_irq_handler      = 0;
	}
        for (i = 0; i < NR_SYSRQS; i++) {
		sysrq[i].rtai_handler = 0;
		sysrq[i].user_handler = 0;
		sysrq[i].label        = 0;
        }
	for (i = 0; i < NR_RTAI_IRQS; i++) {
		global_irq[i].ppc_irq = 0;
		global_irq[i].mapped  = RTAI_IRQ_UNMAPPED;
#ifdef CONFIG_SMP
		global_irq[i].dest_status = 0;
#endif
		global_irq[i].handler = 0;
		global_irq[i].ext = 0;
		global_irq[i].irq_count = 0;
	}
	for (i = 0; i < NR_IRQS; i++) {
		ic_ack_irq[i] = do_nothing_picfun;
		if ((linux_irq_desc_handler[i] = IRQ_DESC[i].handler)) {
			ic_ack_irq[i] = (IRQ_DESC[i].handler)->ack;
		}

		if (IRQ_DESC[i].handler && IRQ_DESC[i].action) {
#ifdef CONFIG_SMP
			if (i < OPENPIC_VEC_IPI) {
				map_global_ppc_irq(i);
			} else {
				map_cpu_own_ppc_irq(i);
			}
#else
			map_global_ppc_irq(i);
#endif
		} else {
			rtai_irq[i] = -1;
		}
	}

	sysrq[1].rtai_handler = rt_printk_sysreq_handler;
	sysrq[1].label = 0x1F0000F1;
#ifdef CONFIG_RTAI_MOUNT_ON_LOAD
	__rt_mount_rtai();
#endif

#ifdef CONFIG_PROC_FS
	rtai_proc_register();
#endif

	return 0;
}

void cleanup_module(void)
{
#ifdef CONFIG_RTAI_MOUNT_ON_LOAD
	__rt_umount_rtai();
#endif

#ifdef CONFIG_PROC_FS
	rtai_proc_unregister();
#endif

	return;
}

/* ----------------------< proc filesystem section >----------------------*/
#ifdef CONFIG_PROC_FS

struct proc_dir_entry *rtai_proc_root = NULL;

static int rtai_read_rtai(char *page, char **start, off_t off, int count,
                          int *eof, void *data)
{
	PROC_PRINT_VARS;
        int i;

        PROC_PRINT("\nRTAI Real Time Kernel, Version: %s\n\n", RTAI_RELEASE);
        PROC_PRINT("    Mount count: %d\n", rtai_mounted);
        PROC_PRINT("    Frequency  : %d\n", FREQ_DECR);
        PROC_PRINT("    Latency    : %d ns\n", LATENCY_DECR);
        PROC_PRINT("    Setup time : %d ns\n", SETUP_TIME_DECR);
        PROC_PRINT("\nGlobal irqs used by RTAI:\n");
        for (i = 0; i <= LAST_GLOBAL_RTAI_IRQ; i++) {
          if (global_irq[i].handler) {
            PROC_PRINT("%3d: %10i\n",
		       global_irq[i].ppc_irq, global_irq[i].irq_count);
          }
        }
#ifdef CONFIG_SMP
        PROC_PRINT("\nCpu_Own irqs used by RTAI: \n");
        for (i = LAST_GLOBAL_RTAI_IRQ + 1; i < NR_RTAI_IRQS; i++) {
          if (global_irq[i].handler) {
            PROC_PRINT("%d ", global_irq[i].ppc_irq);
          }
        }
#endif
        PROC_PRINT("\nRTAI sysreqs in use: \n");
        for (i = 0; i < NR_SYSRQS; i++) {
          if (sysrq[i].rtai_handler || sysrq[i].user_handler) {
            PROC_PRINT("%d ", i);
          }
        }
        PROC_PRINT("\n\n");

	PROC_PRINT_DONE;
}       /* End function - rtai_read_rtai */

static int rtai_proc_register(void)
{

	struct proc_dir_entry *ent;

        rtai_proc_root = create_proc_entry("rtai", S_IFDIR, 0);
        if (!rtai_proc_root) {
		printk("Unable to initialize /proc/rtai\n");
                return(-1);
        }
	rtai_proc_root->owner = THIS_MODULE;
        ent = create_proc_entry("rtai", S_IFREG|S_IRUGO|S_IWUSR, rtai_proc_root);
        if (!ent) {
		printk("Unable to initialize /proc/rtai/rtai\n");
                return(-1);
        }
	ent->read_proc = rtai_read_rtai;
        return(0);
}       /* End function - rtai_proc_register */


static void rtai_proc_unregister(void)
{
        remove_proc_entry("rtai", rtai_proc_root);
        remove_proc_entry("rtai", 0);
}       /* End function - rtai_proc_unregister */

#endif /* CONFIG_PROC_FS */
/* ------------------< end of proc filesystem section >------------------*/


/********** SOME TIMER FUNCTIONS TO BE LIKELY NEVER PUT ELSWHERE *************/

/* Real time timers. No oneshot, and related timer programming, calibration. */
/* Use the utility module. It is also to be decided if this stuff has to     */
/* stay here.                                                                */

struct calibration_data tuned;
struct rt_times rt_times;
struct rt_times rt_smp_times[NR_RT_CPUS];

void rt_request_timer(void (*handler)(void), unsigned int tick, int unused)
{
	unsigned int cpuid;
	RTIME t;
	unsigned long flags;

	TRACE_RTAI_TIMER(TRACE_RTAI_EV_TIMER_REQUEST, handler, tick);

	if (processor[cpuid = hard_cpu_id()].rt_timer_handler) {
		return;
	}
	flags = hard_lock_all();

#ifdef CONFIG_4xx
	/* Disable auto-reload mode used by Linux */
	mtspr(SPRN_TCR,  mfspr(SPRN_TCR) & ~TCR_ARE);
	do {
		t = rdtsc();
	} while (get_dec_4xx() > 0);
#else
	do {
		t = rdtsc();
	} while (get_dec() <= tb_ticks_per_jiffy);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,2)
	disarm_decr[cpuid]=1;
#else
	rtai_regs.trap = 1;
#endif
	rt_times.linux_tick = tb_ticks_per_jiffy;
	rt_times.periodic_tick = tick > 0 && tick < tb_ticks_per_jiffy ? tick : rt_times.linux_tick;
	rt_times.tick_time  = t;
	rt_times.intr_time  = t + rt_times.periodic_tick;
	rt_times.linux_time = t + rt_times.linux_tick;
	processor[cpuid].rt_timer_handler = handler;
	rt_set_decrementer_count(rt_times.periodic_tick);
	hard_unlock_all(flags);
	return;
}

void rt_free_timer(void)
{
	unsigned long flags;

	TRACE_RTAI_TIMER(TRACE_RTAI_EV_TIMER_FREE, 0, 0);

	flags = hard_lock_all();
#ifdef CONFIG_4xx
	/* Restore auto-reload mode for Linux */
	mtspr(SPRN_TCR, mfspr(SPRN_TCR) | TCR_ARE);

	/* Set the PIT reload value and just let it run. */
	mtspr(SPRN_PIT, tb_ticks_per_jiffy);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,2)
	disarm_decr[hard_cpu_id()]=0;
#else
	rtai_regs.trap = 0;
#endif
	processor[hard_cpu_id()].rt_timer_handler = 0;
	hard_unlock_all(flags);
}

void rt_request_apic_timers(void (*handler)(void), struct apic_timer_setup_data *apic_timer_data)
{
	RTIME t;
	int cpuid;
	unsigned long flags;
	struct apic_timer_setup_data *p;
	struct rt_times *rt_times;

	TRACE_RTAI_TIMER(TRACE_RTAI_EV_TIMER_REQUEST_APIC, handler, 0);

	flags = hard_lock_all();
	do {
		t = rdtsc();
	} while (get_dec());
	for (cpuid = 0; cpuid < NR_RT_CPUS; cpuid++) {
		*(p = apic_timer_data + cpuid) = apic_timer_data[cpuid];
		rt_times = rt_smp_times + cpuid;
		p->mode = 1;
		rt_times->linux_tick = tb_ticks_per_jiffy;
		rt_times->tick_time = llimd(t + tb_ticks_per_jiffy, FREQ_DECR, tuned.cpu_freq);
		rt_times->periodic_tick =
		p->count = p->mode > 0 ? imuldiv(p->count, FREQ_DECR, 1000000000) : tb_ticks_per_jiffy;
		rt_times->intr_time = rt_times->tick_time + rt_times->periodic_tick;
		rt_times->linux_time = rt_times->tick_time + rt_times->linux_tick;
		processor[cpuid = hard_cpu_id()].rt_timer_handler = handler;
	}
	hard_unlock_all(flags);
	for (cpuid = 0; cpuid < NR_RT_CPUS; cpuid++) {
		if ((p = apic_timer_data + cpuid)->mode > 0) {
			p->mode = 1;
			p->count = imuldiv(p->count, FREQ_DECR, 1000000000);
		} else {
			p->mode = 0;
			p->count = imuldiv(p->count, tuned.cpu_freq, 1000000000);
		}
	}
}

void rt_free_apic_timers(void)
{
	unsigned long flags, cpuid;

	TRACE_RTAI_TIMER(TRACE_RTAI_EV_TIMER_APIC_FREE, 0, 0);

	flags = hard_lock_all();
	for (cpuid = 0; cpuid < NR_RT_CPUS; cpuid++) {
		processor[cpuid = hard_cpu_id()].rt_timer_handler = 0;
	}
	hard_unlock_all(flags);
}

/******** END OF SOME TIMER FUNCTIONS TO BE LIKELY NEVER PUT ELSWHERE *********/

// Our printk function, its use should be safe everywhere.
#include <linux/console.h>

int rtai_print_to_screen(const char *format, ...)
{
        static spinlock_t display_lock = SPIN_LOCK_UNLOCKED;
        static char display[25*80];
        unsigned long flags;
        struct console *c;
        va_list args;
        int len;

        flags = rt_spin_lock_irqsave(&display_lock);
        va_start(args, format);
        len = vsprintf(display, format, args);
        va_end(args);
        c = console_drivers;
        while(c) {
                if ((c->flags & CON_ENABLED) && c->write)
                        c->write(c, display, len);
                c = c->next;
	}
        rt_spin_unlock_irqrestore(flags, &display_lock);

	return len;
}

/*
 *  rt_printk.c, hacked from linux/kernel/printk.c.
 *
 * Modified for RT support, David Schleef.
 *
 * Adapted to RTAI, and restyled his way by Paolo Mantegazza. Now it has been
 * taken away from the fifos module and has become an integral part of the basic
 * RTAI module.
 */

#define PRINTK_BUF_LEN	(4096)
#define TEMP_BUF_LEN	(256)

static char rt_printk_buf[PRINTK_BUF_LEN];
static int buf_front, buf_back;

static char buf[TEMP_BUF_LEN];

int rt_printk(const char *fmt, ...)
{
        static spinlock_t display_lock = SPIN_LOCK_UNLOCKED;
	va_list args;
	int len, i;
	unsigned long flags;

        flags = rt_spin_lock_irqsave(&display_lock);
	va_start(args, fmt);
	len = vsprintf(buf, fmt, args);
	va_end(args);
	if (buf_front + len >= PRINTK_BUF_LEN) {
		i = PRINTK_BUF_LEN - buf_front;
		memcpy(rt_printk_buf + buf_front, buf, i);
		memcpy(rt_printk_buf, buf + i, len - i);
		buf_front = len - i;
	} else {
		memcpy(rt_printk_buf + buf_front, buf, len);
		buf_front += len;
	}
        rt_spin_unlock_irqrestore(flags, &display_lock);
	rt_pend_linux_srq(1);

	return len;
}

static void rt_printk_sysreq_handler(void)
{
	int tmp;

	while(1) {
		tmp = buf_front;
		if (buf_back  > tmp) {
			printk("%.*s", PRINTK_BUF_LEN - buf_back, rt_printk_buf + buf_back);
			buf_back = 0;
		}
		if (buf_back == tmp) {
			break;
		}
		printk("%.*s", tmp - buf_back, rt_printk_buf + buf_back);
		buf_back = tmp;
	}
}
