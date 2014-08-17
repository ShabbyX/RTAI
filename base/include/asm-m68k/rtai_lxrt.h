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

#ifndef _RTAI_ASM_M68K_LXRT_H
#define _RTAI_ASM_M68K_LXRT_H

#include <linux/version.h>

#include <asm/rtai_vectors.h>

#ifdef CONFIG_RTAI_LXRT_USE_LINUX_SYSCALL
#define USE_LINUX_SYSCALL
#else
#undef USE_LINUX_SYSCALL
#endif

#define RTAI_SYSCALL_NR      0x70000000
#define RTAI_SYSCALL_CODE    d0 //From RTAI_DO_TRAP_SYS
#define RTAI_SYSCALL_ARGS    d1 //From RTAI_DO_TRAP_SYS
#define RTAI_SYSCALL_RETPNT  d2

#define LINUX_SYSCALL_NR      orig_d0
#define LINUX_SYSCALL_REG1    d1
#define LINUX_SYSCALL_REG2    d2
#define LINUX_SYSCALL_REG3    d3
#define LINUX_SYSCALL_REG4    d4
#define LINUX_SYSCALL_REG5    d5
#define LINUX_SYSCALL_REG6    a0
#define LINUX_SYSCALL_RETREG  d0
#define LINUX_SYSCALL_FLAGS   sr

#define LXRT_DO_IMMEDIATE_LINUX_SYSCALL(regs) \
	do { \
		regs->LINUX_SYSCALL_RETREG = sys_call_table[regs->LINUX_SYSCALL_NR](*regs); \
	} while (0)

#define SET_LXRT_RETVAL_IN_SYSCALL(regs, retval) \
	do { \
/*                if (regs->RTAI_SYSCALL_RETPNT) { \
			rt_copy_to_user((void *)regs->RTAI_SYSCALL_RETPNT, &retval, sizeof(retval)); \
		} */\
	} while (0)

#define LOW  0
#define HIGH 1

#if defined(CONFIG_RTAI_RTC_FREQ) && CONFIG_RTAI_RTC_FREQ >= 2

#define TIMER_NAME        "RTC"
#define TIMER_FREQ        CONFIG_RTAI_RTC_FREQ
#define TIMER_LATENCY     0
#define TIMER_SETUP_TIME  0
#define ONESHOT_SPAN      0

#else /* CONFIG_RTAI_RTC_FREQ == 0 */

#define USE_LINUX_TIMER
#define TIMER_NAME        "COLDFIRE TIMER"
#define TIMER_TYPE  0
#define HRT_LINUX_TIMER_NAME  "cf_timer"
#define TIMER_FREQ        RTAI_FREQ_8254
#define TIMER_LATENCY     RTAI_LATENCY_8254
#define TIMER_SETUP_TIME  RTAI_SETUP_TIME_8254
#define ONESHOT_SPAN      (CPU_FREQ/(CONFIG_RTAI_CAL_FREQS_FACT + 2))
extern int ipipe_timer_run_count;
#define update_linux_timer(cpuid) \
do { \
	if (!IS_FUSION_TIMER_RUNNING()) { \
		__ipipe_saved_trr = __raw_readtrr(TA(MCFTIMER_TCN)); \
		hal_pend_uncond(RTAI_TIMER_LINUX_IRQ, cpuid); \
		ipipe_timer_run_count++;\
	} \
} while (0)

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

#define __LXRT_GET_DATASEG(reg) "movel $" STR(__USER_DS) ",%" #reg "\n\t"

static inline void _lxrt_context_switch (struct task_struct *prev, struct task_struct *next, int cpuid)
{
	extern void context_switch(void *, void *, void *);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19)
	prev->fpu_counter = 0;
#endif
	context_switch(0, prev, next);
}

#if 0
#define IN_INTERCEPT_IRQ_ENABLE()   do { rtai_hw_sti(); } while (0)
#define IN_INTERCEPT_IRQ_DISABLE()  do { rtai_hw_cli(); } while (0)
#else
#define IN_INTERCEPT_IRQ_ENABLE()   do { } while (0)
#define IN_INTERCEPT_IRQ_DISABLE()  do { } while (0)
#endif

#include <linux/slab.h>

#define REG_SP ksp
#if 1 // optimised (?)
static inline void kthread_fun_set_jump(struct task_struct *lnxtsk)
{
	lnxtsk->rtai_tskext(TSKEXT2) = kmalloc(sizeof(struct thread_struct)/* + sizeof(struct thread_info)*/ + (lnxtsk->thread.REG_SP & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.REG_SP, GFP_KERNEL);
	*((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2)) = lnxtsk->thread;
//	memcpy(lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), (void *)(lnxtsk->thread.REG_SP & ~(THREAD_SIZE - 1)), sizeof(struct thread_info));
	memcpy(lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct)/* + sizeof(struct thread_info)*/, (void *)(lnxtsk->thread.REG_SP), (lnxtsk->thread.REG_SP & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.REG_SP);
}

static inline void kthread_fun_long_jump(struct task_struct *lnxtsk)
{
	lnxtsk->thread = *((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2));
//	memcpy((void *)(lnxtsk->thread.REG_SP & ~(THREAD_SIZE - 1)), lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), sizeof(struct thread_info));
	memcpy((void *)lnxtsk->thread.REG_SP, lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct)/* + sizeof(struct thread_info)*/, (lnxtsk->thread.REG_SP & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.REG_SP);
}
#else  // brute force
static inline void kthread_fun_set_jump(struct task_struct *lnxtsk)
{
	lnxtsk->rtai_tskext(TSKEXT2) = kmalloc(sizeof(struct thread_struct) + THREAD_SIZE, GFP_KERNEL);
	*((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2)) = lnxtsk->thread;
	memcpy(lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), (void *)(lnxtsk->thread.REG_SP & ~(THREAD_SIZE - 1)), THREAD_SIZE);
}

static inline void kthread_fun_long_jump(struct task_struct *lnxtsk)
{
	lnxtsk->thread = *((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2));
	memcpy((void *)(lnxtsk->thread.REG_SP & ~(THREAD_SIZE - 1)), lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), THREAD_SIZE);
}
#endif

#define rt_copy_from_user(a, b, c)  \
	( { int ret = __copy_from_user_inatomic(a, b, c); ret; } )
#define rt_copy_to_user(a, b, c)  \
	( { int ret = __copy_to_user_inatomic(a, b, c); ret; } )
#define rt_strncpy_from_user(a, b, c)  \
	( { int ret = strncpy_from_user(a, b, c); ret; } )
#define rt_put_user  __put_user
#define rt_get_user  __get_user

//FIXME: Shit may happen here
/*__attribute__((regparm(3)))*/
#define do_notify_resume(regs, _unused, thread_info_flags) \
{ \
	rt_printk("RTAI-do_notify_resume: Shit!!! \n"); \
}
#define RT_DO_SIGNAL(regs)  do_notify_resume(regs, NULL, (_TIF_SIGPENDING | _TIF_RESTORE_SIGMASK));

#else /* !__KERNEL__ */

/* NOTE: Keep the following routines unfold: this is a compiler
   compatibility issue. */

#include <sys/syscall.h>
#include <unistd.h>

//#define CLOCK_MONOTONIC 1
//#define PTHREAD_STACK_MIN 16384

#ifndef CONFIG_MMU
#define mlockall(asd) do {} while (0)
#endif /* CONFIG_MMU */

static union rtai_lxrt_t _rtai_lxrt(int srq, void *arg)
{
	union rtai_lxrt_t retval;
#ifdef USE_LINUX_SYSCALL
	syscall(RTAI_SYSCALL_NR, srq, arg, &retval);
#else 
	RTAI_DO_TRAP_SYS(&retval.rt, srq, (unsigned long)arg);
#endif
	return retval;
}

static inline union rtai_lxrt_t rtai_lxrt(short int dynx, short int lsize, int srq, void *arg)
{
	return _rtai_lxrt(ENCODE_LXRT_REQ(dynx, srq, lsize), arg);
}

#define rtai_iopl()  do { } while (0)

#endif /* __KERNEL__ */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_RTAI_ASM_M68K_LXRT_H */
