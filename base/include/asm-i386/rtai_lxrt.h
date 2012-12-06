/*
 * Copyright (C) 1999-2003 Paolo Mantegazza <mantegazza@aero.polimi.it>
 * extensions for user space modules are jointly copyrighted (2000) with:
 *		Pierre Cloutier <pcloutier@poseidoncontrols.com>,
 *		Steve Papacharalambous <stevep@zentropix.com>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#ifndef _RTAI_ASM_I386_LXRT_H
#define _RTAI_ASM_I386_LXRT_H

#include <linux/version.h>

#include <asm/rtai_vectors.h>

#ifdef CONFIG_RTAI_LXRT_USE_LINUX_SYSCALL
#define USE_LINUX_SYSCALL
#else
#undef USE_LINUX_SYSCALL
#endif

#define RTAI_SYSCALL_NR      0x70000000

#if defined(__KERNEL__) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)

#define RT_REG_ORIG_AX           orig_ax
#define RT_REG_SP                sp
#define RT_REG_FLAGS             flags
#define RT_REG_IP                ip
#define RT_REG_CS                cs
#define RT_REG_BP                bp

#define RTAI_SYSCALL_CODE     bx
#define RTAI_SYSCALL_ARGS     cx
#define RTAI_SYSCALL_RETPNT   dx

#define LINUX_SYSCALL_NR      RT_REG_ORIG_AX
#define LINUX_SYSCALL_REG1    bx
#define LINUX_SYSCALL_REG2    cx
#define LINUX_SYSCALL_REG3    dx
#define LINUX_SYSCALL_REG4    si
#define LINUX_SYSCALL_REG5    di
#define LINUX_SYSCALL_REG6    RT_REG_BP
#define LINUX_SYSCALL_RETREG  ax
#define LINUX_SYSCALL_FLAGS   RT_REG_FLAGS

#else

#define RT_REG_ORIG_AX           orig_eax
#define RT_REG_SP                esp
#define RT_REG_FLAGS             eflags
#define RT_REG_IP                eip
#define RT_REG_CS                xcs
#define RT_REG_BP                ebp

#define RTAI_SYSCALL_CODE     ebx
#define RTAI_SYSCALL_ARGS     ecx
#define RTAI_SYSCALL_RETPNT   edx

#define LINUX_SYSCALL_NR      RT_REG_ORIG_AX
#define LINUX_SYSCALL_REG1    ebx
#define LINUX_SYSCALL_REG2    ecx
#define LINUX_SYSCALL_REG3    edx
#define LINUX_SYSCALL_REG4    esi
#define LINUX_SYSCALL_REG5    edi
#define LINUX_SYSCALL_REG6    RT_REG_BP
#define LINUX_SYSCALL_RETREG  eax
#define LINUX_SYSCALL_FLAGS   RT_REG_FLAGS

#endif

#define LXRT_DO_IMMEDIATE_LINUX_SYSCALL(regs) \
	do { \
		regs->LINUX_SYSCALL_RETREG = sys_call_table[regs->LINUX_SYSCALL_NR](*regs); \
	} while (0)

#define SET_LXRT_RETVAL_IN_SYSCALL(regs, retval) \
	do { \
                if (regs->RTAI_SYSCALL_RETPNT) { \
			rt_copy_to_user((void *)regs->RTAI_SYSCALL_RETPNT, &retval, sizeof(retval)); \
		} \
	} while (0)

#define LOW  0
#define HIGH 1

#if defined(CONFIG_RTAI_RTC_FREQ) && CONFIG_RTAI_RTC_FREQ >= 2

#define TIMER_NAME        "RTC"
#define HRT_LINUX_TIMER_NAME  "rtc"
#define TIMER_FREQ        CONFIG_RTAI_RTC_FREQ
#define TIMER_LATENCY     0
#define TIMER_SETUP_TIME  0
#define ONESHOT_SPAN      0

#else /* CONFIG_RTAI_RTC_FREQ == 0 */

#ifdef CONFIG_X86_LOCAL_APIC

#define TIMER_NAME        "APIC"
#define TIMER_TYPE  1
#define HRT_LINUX_TIMER_NAME  "lapic"
#define FAST_TO_READ_TSC
#define TIMER_FREQ        RTAI_FREQ_APIC
#define TIMER_LATENCY     RTAI_LATENCY_APIC
#define TIMER_SETUP_TIME  RTAI_SETUP_TIME_APIC
#define ONESHOT_SPAN      (CPU_FREQ/(CONFIG_RTAI_CAL_FREQS_FACT + 2)) //(0x7FFFFFFFLL*(CPU_FREQ/TIMER_FREQ))
#ifdef CONFIG_GENERIC_CLOCKEVENTS
#define USE_LINUX_TIMER
#define update_linux_timer(cpuid) \
	do { hal_pend_uncond(LOCAL_TIMER_IPI, cpuid); } while (0)
#else /* !CONFIG_GENERIC_CLOCKEVENTS */
#define update_linux_timer(cpuid)
#endif /* CONFIG_GENERIC_CLOCKEVENTS */

#else /* !CONFIG_X86_LOCAL_APIC */

#define USE_LINUX_TIMER
#define TIMER_NAME        "8254-PIT"
#define TIMER_TYPE  0
#define HRT_LINUX_TIMER_NAME  "pit"
#define TIMER_FREQ        RTAI_FREQ_8254
#define TIMER_LATENCY     RTAI_LATENCY_8254
#define TIMER_SETUP_TIME  RTAI_SETUP_TIME_8254
#define ONESHOT_SPAN      ((0x7FFF*(CPU_FREQ/TIMER_FREQ))/(CONFIG_RTAI_CAL_FREQS_FACT + 1)) //(0x7FFF*(CPU_FREQ/TIMER_FREQ))
#define update_linux_timer(cpuid) \
do { \
	if (!IS_FUSION_TIMER_RUNNING()) { \
		hal_pend_uncond(TIMER_8254_IRQ, cpuid); \
	} \
} while (0)

#endif /* CONFIG_X86_LOCAL_APIC */

#endif /* CONFIG_RTAI_RTC_FREQ != 0 */

union rtai_lxrt_t {
    RTIME rt;
    int i[2];
    void *v[2];
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __KERNEL__

#include <asm/segment.h>
#include <asm/mmu_context.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define __LXRT_GET_DATASEG(reg) "movl $" STR(__KERNEL_DS) ",%" #reg "\n\t"
#else /* KERNEL_VERSION >= 2.6.0 */
#define __LXRT_GET_DATASEG(reg) "movl $" STR(__USER_DS) ",%" #reg "\n\t"
#endif  /* KERNEL_VERSION < 2.6.0 */

static inline void _lxrt_context_switch (struct task_struct *prev, struct task_struct *next, int cpuid)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	extern void context_switch(void *, void *);
	struct mm_struct *oldmm = prev->active_mm;
	switch_mm(oldmm, next->active_mm, next, cpuid);
	if (!next->mm) enter_lazy_tlb(oldmm, next, cpuid);
	context_switch(prev, next); // was switch_to(prev, next, prev);
#else /* >= 2.6.0 */
	extern void context_switch(void *, void *, void *);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19)
	prev->fpu_counter = 0;
#endif
	context_switch(0, prev, next);
#endif /* < 2.6.0 */
}

#if 0
#define IN_INTERCEPT_IRQ_ENABLE()   do { rtai_hw_sti(); } while (0)
#define IN_INTERCEPT_IRQ_DISABLE()  do { rtai_hw_cli(); } while (0)
#else
#define IN_INTERCEPT_IRQ_ENABLE()   do { } while (0)
#define IN_INTERCEPT_IRQ_DISABLE()  do { } while (0)
#endif

#include <linux/slab.h>

#if 1 // optimised (?)
static inline void kthread_fun_set_jump(struct task_struct *lnxtsk)
{
	lnxtsk->rtai_tskext(TSKEXT2) = kmalloc(sizeof(struct thread_struct)/* + sizeof(struct thread_info)*/ + (lnxtsk->thread.RT_REG_SP & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.RT_REG_SP, GFP_KERNEL);
	*((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2)) = lnxtsk->thread;
//	memcpy(lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), (void *)(lnxtsk->thread.RT_REG_SP & ~(THREAD_SIZE - 1)), sizeof(struct thread_info));
	memcpy(lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct)/* + sizeof(struct thread_info)*/, (void *)(lnxtsk->thread.RT_REG_SP), (lnxtsk->thread.RT_REG_SP & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.RT_REG_SP);
}

static inline void kthread_fun_long_jump(struct task_struct *lnxtsk)
{
	lnxtsk->thread = *((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2));
//	memcpy((void *)(lnxtsk->thread.RT_REG_SP & ~(THREAD_SIZE - 1)), lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), sizeof(struct thread_info));
	memcpy((void *)lnxtsk->thread.RT_REG_SP, lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct)/* + sizeof(struct thread_info)*/, (lnxtsk->thread.RT_REG_SP & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.RT_REG_SP);
}
#else  // brute force
static inline void kthread_fun_set_jump(struct task_struct *lnxtsk)
{
	lnxtsk->rtai_tskext(TSKEXT2) = kmalloc(sizeof(struct thread_struct) + THREAD_SIZE, GFP_KERNEL);
	*((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2)) = lnxtsk->thread;
	memcpy(lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), (void *)(lnxtsk->thread.RT_REG_SP & ~(THREAD_SIZE - 1)), THREAD_SIZE);
}

static inline void kthread_fun_long_jump(struct task_struct *lnxtsk)
{
	lnxtsk->thread = *((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2));
	memcpy((void *)(lnxtsk->thread.RT_REG_SP & ~(THREAD_SIZE - 1)), lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), THREAD_SIZE);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define rt_copy_from_user     __copy_from_user
#define rt_copy_to_user       __copy_to_user
#define rt_strncpy_from_user  strncpy_from_user
#else
#define rt_copy_from_user(a, b, c)  \
	( { int ret = __copy_from_user_inatomic(a, b, c); ret; } )
#define rt_copy_to_user(a, b, c)  \
	( { int ret = __copy_to_user_inatomic(a, b, c); ret; } )
#define rt_strncpy_from_user(a, b, c)  \
	( { int ret = strncpy_from_user(a, b, c); ret; } )
#endif
#define rt_put_user  __put_user
#define rt_get_user  __get_user

//#define RTAI_DO_LINUX_SIGNAL
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
extern int FASTCALL(do_signal(struct pt_regs *regs, sigset_t *oldset));
#define RT_DO_SIGNAL(regs)  do_signal(regs, NULL)
#else
__attribute__((regparm(3))) void do_notify_resume(struct pt_regs *regs, void *_unused, __u32 thread_info_flags);
#define RT_DO_SIGNAL(regs)  do_notify_resume(regs, NULL, (_TIF_SIGPENDING | _TIF_RESTORE_SIGMASK));
#endif

#else /* !__KERNEL__ */

/* NOTE: Keep the following routines unfold: this is a compiler
   compatibility issue. */

#include <sys/syscall.h>
#include <unistd.h>

static union rtai_lxrt_t _rtai_lxrt(int srq, void *arg)
{
	union rtai_lxrt_t retval;
#ifdef USE_LINUX_SYSCALL
	syscall(RTAI_SYSCALL_NR, srq, arg, &retval);
#else 
	RTAI_DO_TRAP(RTAI_SYS_VECTOR, retval, srq, arg);
#endif
	return retval;
}

static inline union rtai_lxrt_t rtai_lxrt(short int dynx, short int lsize, int srq, void *arg)
{
	return _rtai_lxrt(ENCODE_LXRT_REQ(dynx, srq, lsize), arg);
}

#define rtai_iopl()  do { extern int iopl(int); iopl(3); } while (0)

#endif /* __KERNEL__ */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_RTAI_ASM_I386_LXRT_H */
