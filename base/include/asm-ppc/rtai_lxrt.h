/*
 * Copyright (C) 1999-2007 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *		   2000 Pierre Cloutier <pcloutier@poseidoncontrols.com>
		   2002 Steve Papacharalambous <stevep@zentropix.com>
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

#ifndef _RTAI_ASM_PPC_LXRT_H
#define _RTAI_ASM_PPC_LXRT_H

#include <linux/version.h>

#include <asm/rtai_vectors.h>

#ifndef CONFIG_RTAI_INTERNAL_LXRT_SUPPORT
#define CONFIG_RTAI_INTERNAL_LXRT_SUPPORT 1
#endif

#ifdef CONFIG_RTAI_LXRT_USE_LINUX_SYSCALL
#define USE_LINUX_SYSCALL
#else
#undef USE_LINUX_SYSCALL
#endif

#define LINUX_SYSCALL_NR      gpr[0]
#define LINUX_SYSCALL_REG1    gpr[3]
#define LINUX_SYSCALL_REG2    gpr[4]
#define LINUX_SYSCALL_REG3    gpr[5]
#define LINUX_SYSCALL_REG4    gpr[6]
#define LINUX_SYSCALL_REG5    gpr[7]
#define LINUX_SYSCALL_REG6    gpr[8]
#define LINUX_SYSCALL_RETREG  gpr[3]
#define LINUX_SYSCALL_FLAGS   msr

#define RTAI_SYSCALL_NR      0x70000000
#define RTAI_SYSCALL_CODE    LINUX_SYSCALL_REG1
#define RTAI_SYSCALL_ARGS    LINUX_SYSCALL_REG2
#define RTAI_SYSCALL_RETPNT  LINUX_SYSCALL_REG3

//#define RTAI_FAKE_LINUX_SYSCALL  39

//#define NR_syscalls __NR_syscall_max

#define LXRT_DO_IMMEDIATE_LINUX_SYSCALL(regs) \
        do { \
		regs->LINUX_SYSCALL_RETREG = ((asmlinkage int (*)(long, ...))sys_call_table[regs->LINUX_SYSCALL_NR])(regs->LINUX_SYSCALL_REG1, regs->LINUX_SYSCALL_REG2, regs->LINUX_SYSCALL_REG3, regs->LINUX_SYSCALL_REG4, regs->LINUX_SYSCALL_REG5, regs->LINUX_SYSCALL_REG6); \
        } while (0)

#define SET_LXRT_RETVAL_IN_SYSCALL(regs, retval) \
        do { \
		if (regs->RTAI_SYSCALL_RETPNT) { \
			rt_copy_to_user((void *)regs->RTAI_SYSCALL_RETPNT, &retval, sizeof(retval)); \
		} \
        } while (0)

#define LOW   0
#define HIGH  1

#define USE_LINUX_TIMER
#define TIMER_NAME        "DECREMENTER"
#define TIMER_FREQ        RTAI_FREQ_8254
#define TIMER_LATENCY     RTAI_LATENCY_8254
#define TIMER_SETUP_TIME  RTAI_SETUP_TIME_8254
#define ONESHOT_SPAN      ((0x7FFF*(CPU_FREQ/TIMER_FREQ))/(CONFIG_RTAI_CAL_FREQS_FACT + 1)) //(0x7FFF*(CPU_FREQ/TIMER_FREQ))

#define update_linux_timer(cpuid) \
	do { rtai_disarm_decr(cpuid, 1); hal_pend_uncond(TIMER_8254_IRQ, cpuid); } while (0)

union rtai_lxrt_t
{
	RTIME rt;
	long i[2];
	void *v[2];
};

#ifndef THREAD_SIZE
#define THREAD_SIZE  8192    /* 2 pages */
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __KERNEL__

#include <linux/slab.h>
#include <asm/mmu_context.h>

static inline void _lxrt_context_switch (struct task_struct *prev, struct task_struct *next, int cpuid)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	extern void context_switch(void *, void *);
	struct mm_struct *oldmm = prev->active_mm;
	switch_mm(oldmm, next->active_mm, next, cpuid);
	if (!next->mm)
	{
		enter_lazy_tlb(oldmm, next, cpuid);
	}
	context_switch(prev, next);
#else /* >= 2.6.0 */
	extern void *context_switch(void *, void *, void *);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18)
	prev->fpu_counter = 0;
#endif
	context_switch(NULL, prev, next);
#endif /* < 2.6.0 */
}

#define IN_INTERCEPT_IRQ_ENABLE()   do { } while (0)
#define IN_INTERCEPT_IRQ_DISABLE()  do { } while (0)

static inline void kthread_fun_set_jump(struct task_struct *lnxtsk)
{
	lnxtsk->rtai_tskext(TSKEXT2) = kmalloc(sizeof(struct thread_struct) + (lnxtsk->thread.ksp & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.ksp, GFP_KERNEL);
	*((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2)) = lnxtsk->thread;
	memcpy(lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), (void *)(lnxtsk->thread.ksp), (lnxtsk->thread.ksp & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.ksp);
}

static inline void kthread_fun_long_jump(struct task_struct *lnxtsk)
{
	lnxtsk->thread = *((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2));
	memcpy((void *)lnxtsk->thread.ksp, lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct)/* + sizeof(struct thread_info)*/, (lnxtsk->thread.ksp & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.ksp);
}

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
//extern int FASTCALL(do_signal(sigset_t *oldset, struct pt_regs *regs));
#define RT_DO_SIGNAL(regs)  do_signal(NULL, regs)

#else /* !__KERNEL__ */

#include <sys/syscall.h>
#include <unistd.h>

static inline union rtai_lxrt_t __rtai_lxrt(unsigned long arg0, unsigned long arg1, unsigned long arg2)
{
	union rtai_lxrt_t __sc_ret;
	{
		register unsigned long __sc_0  __asm__ ("r0");
		register unsigned long __sc_3  __asm__ ("r3");
		register unsigned long __sc_4  __asm__ ("r4");

		__sc_0 = arg0;
		__sc_3 = arg1;
		__sc_4 = arg2;

		__asm__ __volatile__
		("sc           \n\t"
		 : "=&r" (__sc_0),
		 "=&r" (__sc_3),  "=&r" (__sc_4)
		 : "0" (__sc_0),
		 "1" (__sc_3), "2" (__sc_4)
		 : "cr0", "ctr", "memory",
		 "r9", "r10","r11", "r12");

		__sc_ret.i[0] = __sc_3;
		__sc_ret.i[1] = __sc_4;
	}
	return __sc_ret;
}

static union rtai_lxrt_t _rtai_lxrt(long srq, void *arg)
{
	union rtai_lxrt_t retval;
#if 1 //def USE_LINUX_SYSCALL
	syscall(RTAI_SYSCALL_NR, srq, arg, &retval);
#else
	retval = __rtai_lxrt(RTAI_SYSCALL_NR, srq, arg);
#endif
	return retval;
}

static inline union rtai_lxrt_t rtai_lxrt(long dynx, long lsize, long srq, void *arg)
{
	return _rtai_lxrt(ENCODE_LXRT_REQ(dynx, srq, lsize), arg);
}

#define rtai_iopl()  do { } while (0)

#endif /* __KERNEL__ */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_RTAI_ASM_PPC_LXRT_H */

#if 0 // to be checked
#include <linux/slab.h>

#if 0 // optimised (?)
static inline void kthread_fun_set_jump(struct task_struct *lnxtsk)
{
	lnxtsk->rtai_tskext(TSKEXT2) =
		kmalloc(sizeof(struct thread_struct) + (lnxtsk->thread.esp & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.esp,
			GFP_KERNEL);
	*((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2)) = lnxtsk->thread;
	memcpy(lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct)/* + sizeof(struct thread_info)*/,
	       (void *)(lnxtsk->thread.esp),
	       (lnxtsk->thread.esp & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.esp);
}

static inline void kthread_fun_long_jump(struct task_struct *lnxtsk)
{
	lnxtsk->thread = *((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2));
	memcpy((void *)lnxtsk->thread.esp,
	       lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct)/* + sizeof(struct thread_info)*/,
	       (lnxtsk->thread.esp & ~(THREAD_SIZE - 1)) + THREAD_SIZE - lnxtsk->thread.esp);
}
#else  // brute force
#include <asm/thread_info.h>

//this solution could be used in i386 too...

// This function save the current thread_struct and stack of the task
static inline void kthread_fun_set_jump(struct task_struct *lnxtsk)
{
	lnxtsk->rtai_tskext(TSKEXT2) = kmalloc(sizeof(struct thread_struct) + THREAD_SIZE, GFP_KERNEL);
	*((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2)) = lnxtsk->thread;
// Page 85 Understanding the Linux Kernel tell that all the architecture stack have at the bottom the struct thread_info so...
// GPR is the small data area pointer and point to the thread_info of the process
// note: this copy the thread_info struct too
	memcpy(lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), (void*) lnxtsk->thread_info, THREAD_SIZE);
}

// This function RESTORE the thread_struct and stack of the task
static inline void kthread_fun_long_jump(struct task_struct *lnxtsk)
{
	lnxtsk->thread = *((struct thread_struct *)lnxtsk->rtai_tskext(TSKEXT2));
// this copy the thread_info struct too
	memcpy((void*) lnxtsk->thread_info, lnxtsk->rtai_tskext(TSKEXT2) + sizeof(struct thread_struct), THREAD_SIZE);
}
#endif
#endif
