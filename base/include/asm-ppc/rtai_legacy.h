/*
COPYRIGHT (C) 2000  Paolo Mantegazza (mantegazza@aero.polimi.it)

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


#ifndef _RTAI_ASM_PPC_RTAI_H_
#define _RTAI_ASM_PPC_RTAI_H_

#include <rtai_types.h>

// These are truly PPC specific.
#define LATENCY_DECR 2500 
#define SETUP_TIME_DECR	500 

// CPU frequency calibration
#define CPU_FREQ (tuned.cpu_freq)
#define FREQ_DECR CPU_FREQ
#define CALIBRATED_CPU_FREQ     0 // Use this if you know better than Linux!

// Do not be messed up by macros names below, is a trick for keeping i386 code.
#define FREQ_8254 FREQ_DECR
#define FREQ_APIC FREQ_DECR
#define LATENCY_8254 3000
#define SETUP_TIME_8254 500
#define TIMER_8254_IRQ 0xFFFFFFFF

#define IFLAG 15

#define RTAI_NR_TRAPS 32

// These and the related vector are of no use, at the moment.
#define RTAI_1_IPI  6
#define RTAI_2_IPI  7
#define RTAI_3_IPI  8
#define RTAI_4_IPI  9

#define RTAI_1_VECTOR  0xD9
#define RTAI_2_VECTOR  0xE1
#define RTAI_3_VECTOR  0xE9
#define RTAI_4_VECTOR  0xF1

#define RT_TIME_END 0x7FFFFFFFFFFFFFFFLL

#ifdef INLINE_MATH
unsigned long long ullmul(unsigned long m0, unsigned long m1);
unsigned long long ulldiv(unsigned long long ull, unsigned long uld, unsigned long *r);
int imuldiv(int i, int mult, int div);
unsigned long long llimd(unsigned long long ull, unsigned long mult, unsigned long div);
#else /* !INLINE_MATH */
// One of the silly thing of 32 bits PPCs, no 64 bits result for 32 bits mul.
static inline unsigned long long ullmul(unsigned long m0, unsigned long m1)
{
	unsigned long long res;

	__asm__ __volatile__ ("mulhwu %0, %1, %2"
	: "=r" (((unsigned long *)&res)[0]) : "%r" (m0), "r" (m1));
	((unsigned long *)&res)[1] = m0*m1;

	return res;

}

// One of the silly thing of 32 bits PPCs, no 64 by 32 bits divide.
static inline unsigned long long ulldiv(unsigned long long ull, unsigned long uld, unsigned long *r)
{
	unsigned long long q, rf;
	unsigned long qh, rh, ql, qf;

	q = 0;
	rf = (unsigned long long)(0xFFFFFFFF - (qf = 0xFFFFFFFF / uld) * uld) + 1ULL;

	while (ull >= uld) {
		((unsigned long *)&q)[0] += (qh = ((unsigned long *)&ull)[0] / uld);
		rh = ((unsigned long *)&ull)[0] - qh * uld;
		q += rh * (unsigned long long)qf + (ql = ((unsigned long *)&ull)[1] / uld);
		ull = rh * rf + (((unsigned long *)&ull)[1] - ql * uld);
	}

	*r = ull;
	return q;
}

static inline int imuldiv(int i, int mult, int div)
{
	unsigned long q, r;

	q = ulldiv(ullmul(i, mult), div, &r);

	return (r + r) > div ? q + 1 : q;
}

static inline unsigned long long llimd(unsigned long long ull, unsigned long mult, unsigned long div)
{
	unsigned long long low;
	unsigned long q, r;

	low  = ullmul(((unsigned long *)&ull)[1], mult);	
	q = ulldiv( ullmul(((unsigned long *)&ull)[0], mult) + ((unsigned long *)&low)[0], div, (unsigned long *)&low);
	low = ulldiv(low, div, &r);
	((unsigned long *)&low)[0] += q;

	return (r + r) > div ? low + 1 : low;
}
#endif /* INLINE_MATH */

#ifdef __KERNEL__

#ifndef __cplusplus
#include <linux/types.h>
#include <linux/rtc.h>          /* to avoid warnings in time.h */
#include <asm/time.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/irq.h>
#include <asm/page.h>
#include <asm/ptrace.h>
#include <asm/hw_irq.h>
#include <asm/processor.h>
#include <asm/bitops.h>
#include <asm/rtai_fpu.h>
#include <asm/rtai_atomic.h>
#endif /* !__cplusplus */

// Write to parallel port if present
#if defined(CONFIG_PPC)
#define LPT_OUTB(v) do { } while (0)
#else
#define LPT_OUTB(v) do { outb(v,0x378); } while(0)
#endif

struct apic_timer_setup_data { int mode, count; };

struct desc_struct { void *fun; };

extern unsigned volatile int *locked_cpus;

extern void send_ipi_shorthand(unsigned int shorthand, int irq);
extern void send_ipi_logical(unsigned long dest, int irq);
#define rt_assign_irq_to_cpu(irq, cpu)
#define rt_reset_irq_to_sym_mode(irq)
extern int  rt_request_global_irq(unsigned int irq, void (*handler)(unsigned int irq));
extern int  rt_request_global_irq_arg(unsigned int irq,
		int (*handler)(int,void *,struct pt_regs *),
		unsigned long flags,const char *dev,void *dev_id);
extern int rt_request_global_irq_ext(unsigned int irq, 
				     int (*handler)(unsigned int irq, unsigned long handler), 
				     unsigned long data);
extern void rt_set_global_irq_ext(unsigned int irq, int ext, unsigned long data);
extern int  rt_free_global_irq(unsigned int irq);
extern void rt_ack_irq(unsigned int irq);
extern void rt_mask_and_ack_irq(unsigned int irq);
extern void rt_unmask_irq(unsigned int irq);
extern unsigned int rt_startup_irq(unsigned int irq);
extern void rt_shutdown_irq(unsigned int irq);
extern void rt_enable_irq(unsigned int irq);
extern void rt_disable_irq(unsigned int irq);
extern int rt_request_linux_irq(unsigned int irq,
	void (*linux_handler)(int irq, void *dev_id, struct pt_regs *regs), 
	char *linux_handler_id, void *dev_id);
extern int rt_free_linux_irq(unsigned int irq, void *dev_id);
extern void rt_pend_linux_irq(unsigned int irq);
extern void rt_tick_linux_timer(void);
extern struct desc_struct rt_set_full_intr_vect(unsigned int vector, int type, int dpl, void *handler);
extern void rt_reset_full_intr_vect(unsigned int vector, struct desc_struct idt_element);
#define rt_set_intr_handler(vector, handler) ((void *)0)
#define rt_reset_intr_handler(vector, handler)
extern int rt_request_srq(unsigned int label, void (*rtai_handler)(void), long long (*user_handler)(unsigned int whatever));
extern int rt_free_srq(unsigned int srq);
extern void rt_pend_linux_srq(unsigned int srq);
#define rt_request_cpu_own_irq(irq, handler) rt_request_global_irq((irq), (handler))
#define rt_free_cpu_own_irq(irq) rt_free_global_irq((irq))
extern void rt_request_timer(void (*handler)(void), unsigned int tick, int apic);
extern void rt_free_timer(void);
extern void rt_request_apic_timers(void (*handler)(void), struct apic_timer_setup_data *apic_timer_data);
extern void rt_free_apic_timers(void);
extern void rt_mount_rtai(void);
extern void rt_umount_rtai(void);
extern int rt_printk(const char *format, ...);
extern int rtai_print_to_screen(const char *format, ...);
extern void rt_switch_to_linux(int cpuid);
extern void rt_switch_to_real_time(int cpuid);

#ifndef DEBUG_FLAGS
#define debug_flags_set(ptr,a)
#define debug_flags_check(ptr)
#else
void debug_flags_set(void *ptr,unsigned long flags);
void debug_flags_check(void *ptr);
#endif

static inline void hard_cli(void)
{
	debug_flags_set(__builtin_return_address(0),0);
	__asm__ __volatile__ (
		"\tmfmsr	0\n"
		//"\trlwinm	3,0,16+1,32-1,31\n"
		"\trlwinm	0,0,0,17,15\n"
		"\tmtmsr	0\n"
		: : : "r0"
	);
}


static inline void hard_restore_flags(unsigned long flags)
{
	debug_flags_set(__builtin_return_address(0),flags);
	__asm__ __volatile__ (
		"\tmfmsr 	0\n"
		"\trlwimi	%0,0,0,17,15\n"
		"\tmtmsr	%0\n"
	: : "r" (flags) : "r0"
	);
}
	
static inline void hard_sti(void)
{
	debug_flags_set(__builtin_return_address(0),MSR_EE);
	__asm__ __volatile__ (
		"\tmfmsr	0\n"
		"\tori		0,0,(1<<15)\n"
		"\tmtmsr	0\n"
		: : : "r0"
	);
}

static inline void __hard_save_flags(unsigned long *flags)
{
	__asm__ __volatile__ (
		"\tmfmsr	%0\n"
		: "=r" (*flags)
	);
}

#define hard_save_flags(flags)         do { __hard_save_flags(&(flags)); } while (0)
#define hard_save_flags_and_cli(flags) do { __hard_save_flags(&(flags)); hard_cli(); } while (0)

#ifdef CONFIG_SMP

#define rt_spin_lock(lock) spin_lock((lock))

#define rt_spin_unlock(lock) spin_unlock((lock))

#define hard_cpu_id()  hard_smp_processor_id() 

static inline void rt_get_global_lock(void)
{
	hard_cli();
	if (!test_and_set_bit(hard_cpu_id(), locked_cpus)) {
		while (test_and_set_bit(31, locked_cpus));
	}
}

static inline void rt_release_global_lock(void)
{
	hard_cli();
	if (test_and_clear_bit(hard_cpu_id(), locked_cpus)) {
		clear_bit(31, locked_cpus);
	}
}

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// If NR_RT_CPUS > 8 RTAI must be changed as it cannot use APIC flat delivery
// and the way processor[?].intr_flag is used must be changed (right now it
// exploits the fact that the IF flags is at bit 16 so that bits 0-7 are used
// to mark a cpu as Linux soft irq enabled/disabled. Bad but comfortable, it 
// will take a very very long time before I'll have available SMP with more
// than 8 cpus. Right now they are only:
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#define NR_RT_CPUS  2

#else  /* !CONFIG_SMP */

#define rt_spin_lock(whatever)  
#define rt_spin_unlock(whatever)

#define rt_get_global_lock()  hard_cli()
#define rt_release_global_lock()

#define hard_cpu_id()  0

#define NR_RT_CPUS  1

#endif	/* CONFIG_SMP */

static inline void rt_spin_lock_irq(spinlock_t *lock)          
{
	hard_cli(); 
	rt_spin_lock(lock);
}

static inline void rt_spin_unlock_irq(spinlock_t *lock)
{
	rt_spin_unlock(lock);
	hard_sti();
}

// Note that the spinlock calling convention below for irqsave/restore is 
// sligtly different from the one used in Linux. Done on purpose to get an 
// error if you use Linux spinlocks in real time applications as they do not
// guaranty any protection because of the soft irq disable. Be careful and 
// sure to call the other spinlocks the right way, as they are compatible 
// with Linux.

static inline unsigned int rt_spin_lock_irqsave(spinlock_t *lock)          
{
	unsigned long flags;
	hard_save_flags_and_cli(flags);
	rt_spin_lock(lock);
	return flags;
}

static inline void rt_spin_unlock_irqrestore(unsigned long flags, spinlock_t *lock)
{
	rt_spin_unlock(lock);
	hard_restore_flags(flags);
}

/* Global interrupts and flags control (simplified, and modified, version of */
/* similar global stuff in Linux irq.c).                                     */

static inline void rt_global_cli(void)
{
	rt_get_global_lock();
}

static inline void rt_global_sti(void)
{
	rt_release_global_lock();
	hard_sti();
}

static inline int rt_global_save_flags_and_cli(void)
{
	unsigned long flags;

	hard_save_flags_and_cli(flags);
	if (!test_and_set_bit(hard_cpu_id(), locked_cpus)) {
		while (test_and_set_bit(31, locked_cpus));
		return ((flags & (1 << IFLAG)) + 1);
	} else {
		return (flags & (1 << IFLAG));
	}
}

static inline void rt_global_save_flags(unsigned long *flags)
{
	unsigned long hflags, rflags;

	hard_save_flags_and_cli(hflags);
	hflags = hflags & (1 << IFLAG);
	rflags = hflags | !test_bit(hard_cpu_id(), locked_cpus);
	if (hflags) {
		hard_sti();
	}
	*flags = rflags;
}

static inline void rt_global_restore_flags(unsigned long flags)
{
	switch (flags) {
		case (1 << IFLAG) | 1:	rt_release_global_lock();
		        	  	hard_sti();
					break;
		case (1 << IFLAG) | 0:	rt_get_global_lock();
				 	hard_sti();
					break;
		case (0 << IFLAG) | 1:	rt_release_global_lock();
					break;
		case (0 << IFLAG) | 0:	rt_get_global_lock();
					break;
	}
}

static inline RT_TRAP_HANDLER rt_set_rtai_trap_handler(RT_TRAP_HANDLER handler)
{
	return (RT_TRAP_HANDLER) 0;
}

struct calibration_data {
	unsigned int cpu_freq;
	int latency;
	int setup_time_TIMER_CPUNIT;
	int setup_time_TIMER_UNIT;
	int timers_tol[NR_RT_CPUS];
};

extern struct rt_times rt_times;
extern struct rt_times rt_smp_times[NR_RT_CPUS];
extern struct calibration_data tuned;

#if 0
static inline unsigned int get_dec(void)
{
        return (mfspr(SPRN_DEC));
}

static inline void set_dec(unsigned int val)
{
	mtspr(SPRN_DEC, val);
}
#endif
#ifdef CONFIG_4xx
static inline unsigned int get_dec_4xx(void)
{
	return (mfspr(SPRN_PIT));
}

static inline void set_dec_4xx(unsigned int val)
{
	mtspr(SPRN_PIT, val);
}
#endif

#if 0
#define DECLR_8254_TSC_EMULATION
#define TICK_8254_TSC_EMULATION
#define SETUP_8254_TSC_EMULATION
#define CLEAR_8254_TSC_EMULATION
#endif

static inline unsigned long long rdtsc(void)
{
	unsigned long long ts;
	unsigned long chk;
// The code below is as suggested in Motorola reference manual for 32 bits PPCs.
	__asm__ __volatile__ ("1: mftbu %0; mftb %1; mftbu %2; cmpw %2,%0; bne 1b"
	: "=r" (((unsigned long *)&ts)[0]), "=r" (((unsigned long *)&ts)[1]), "=r" (chk) );

	return ts;
}

#define RT_BUG() do{hard_cli();BUG();}while(0)

static inline void rt_set_decrementer_count(int delay)
{
// NOTE: delay MUST be 0 if a periodic timer is being used.
	if(!delay)delay = rt_times.intr_time - rdtsc();

	if(delay<1){RT_BUG();}
#ifdef CONFIG_4xx
	set_dec_4xx(delay);
#else
        set_dec(delay);
#endif
}

// We like keeping it as for i386.
static inline int ffnz(long ul)
{
	__asm__ __volatile__ ("cntlzw %0, %1" : "=r" (ul) : "r" (ul & (-ul)));

	return 31 - ul;
}

#define rt_set_timer_delay(x)  rt_set_decrementer_count(x)

#endif /* __KERNEL__ */

#define RTAI_DEFAULT_TICK    200000
#define RTAI_DEFAULT_STACKSZ 2000

#endif /* !_RTAI_ASM_PPC_RTAI_H_ */
