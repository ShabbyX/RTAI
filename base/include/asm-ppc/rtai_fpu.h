/*
COPYRIGHT (C) 2000-2007  Paolo Mantegazza (mantegazza@aero.polimi.it)

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

#ifndef _RTAI_ASM_PPC_FPU_H_
#define _RTAI_ASM_PPC_FPU_H_

#include <asm/processor.h>

typedef struct ppc_fpu_env { unsigned long fpu_reg[66]; } FPU_ENV;

#ifdef CONFIG_RTAI_FPU_SUPPORT
/*
 * Saving/restoring the FPU environment in PPC is like eating a cake, very simple. Just save/restore all of the floating
 * point registers, recall they are always 64 bits long, and the floating point state register. Remark: at task init we
 * always enable FP, i.e. MSR flag FP set to 1, for real time tasks and accept default actions for faulty FLOPs, i.e. MSR
 * flags FE0 and FE1 are set to zero.
 */

#define save_fpcr_and_enable_fpu(cr0) \
	do {  \
		__asm__ __volatile__ ("mfmsr %0": "=r"(cr0)); \
		__asm__ __volatile__ ("mtmsr %0":         : "r"(cr0 | MSR_FP): "memory"); \
	} while (0)

#define restore_fpcr(cr0) \
	do { __asm__ __volatile__ ("mtmsr %0": : "r"(cr0)); } while (0)

extern void __save_fpenv(void *fpenv);
#define save_fpenv(x) __save_fpenv(&(x))

extern void __restore_fpenv(void *fpenv);
#define restore_fpenv(x) __restore_fpenv(&(x))

#define restore_task_fpenv(t) \
	do { \ restore_fpenv((t)->thread.fpr); \ } while (0)

#define restore_fpenv_lxrt(t) restore_task_fpenv(t)

#define restore_fpu(tsk) \
	do { \
		giveup_fpu(last_task_used_math); \
		__restore_fpenv((tsk)->thread.fpr); \
		last_task_used_math = tsk; \
	} while (0)

#define init_fpu(tsk) \
	do { restore_fpu(tsk); } while(0)

//#define lnxtsk_uses_fpu(lnxtsk)  (1)

#else

#define enable_fpu()
#define save_fpcr_and_enable_fpu(x)
#define restore_fpcr(x)
#define save_fpenv(x)
#define restore_fpenv(x)
#define restore_task_fpenv(t)
#define restore_fpenv_lxrt(t)
#define restore_fpu(tsk)
#define init_fpu(tsk)
//#define lnxtsk_uses_fpu(lnxtsk)  (0)

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)

#define clear_lnxtsk_uses_fpu(lnxtsk) \
	do { (lnxtsk)->used_math = 0; } while(0)
#define lnxtsk_uses_fpu(lnxtsk)  ((lnxtsk)->used_math)

#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) */

#define clear_lnxtsk_uses_fpu(lnxtsk) \
	do { clear_stopped_child_used_math(lnxtsk); } while(0)
#define lnxtsk_uses_fpu(lnxtsk)  (tsk_used_math(lnxtsk))

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) */

#define set_tsk_used_fpu(t)  do {  } while(0)

#define init_hard_fpu(lnxtsk)  restore_fpu(lnxtsk)

#endif
