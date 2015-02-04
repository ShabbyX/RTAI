/**
 *   @ingroup hal
 *   @file
 *
 *   Copyright: 2013-2015 Paolo Mantegazza.
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

#ifndef _RTAI_ASM_X86_FPU_H
#define _RTAI_ASM_X86_FPU_H

#ifdef __i386__
#include "rtai_fpu_32.h"
#else
#include "rtai_fpu_64.h"
#endif

#ifdef CONFIG_RTAI_FPU_SUPPORT

// FPU MANAGEMENT DRESSED FOR IN KTHREAD/THREAD/PROCESS FPU USAGE FROM RTAI

// Macros used for user space, where Linux might use eother a pointer or the FPU env union
#define init_hard_fpu(lnxtsk)  do { \
	init_hard_fpenv(); \
	set_lnxtsk_uses_fpu(lnxtsk); \
	set_lnxtsk_using_fpu(lnxtsk); \
} while (0)

#define init_fpu(lnxtsk)  do { \
	__init_fpenv(TASK_FPENV(lnxtsk)); \
	set_lnxtsk_uses_fpu(lnxtsk); \
} while (0)

#define restore_fpu(lnxtsk)  do { \
	enable_fpu(); \
	__restore_fpenv(TASK_FPENV(lnxtsk)); \
	set_lnxtsk_using_fpu(lnxtsk); \
} while (0)

#else /* !CONFIG_RTAI_FPU_SUPPORT */

#define enable_fpu()
#define save_fpcr_and_enable_fpu(fpcr)
#define restore_fpcr(fpcr)
#define init_hard_fpenv()
#define init_fpenv(fpenv)
#define save_fpenv(fpenv)
#define restore_fpenv(fpenv)
#define init_hard_fpu(lnxtsk)
#define init_fpu(lnxtsk)
#define restore_fpu(lnxtsk)

#endif /* CONFIG_RTAI_FPU_SUPPORT */

#define set_lnxtsk_uses_fpu(lnxtsk) \
	do { set_stopped_child_used_math(lnxtsk); } while(0)
#define clear_lnxtsk_uses_fpu(lnxtsk) \
	do { clear_stopped_child_used_math(lnxtsk); } while(0)
#define lnxtsk_uses_fpu(lnxtsk)  (tsk_used_math(lnxtsk))

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
#undef init_fpu
#include <asm/i387.h>
#include <asm/fpu-internal.h>
#define rtai_set_fpu_used(lnxtsk) __thread_set_has_fpu(lnxtsk)
#else
#define rtai_set_fpu_used(lnxtsk) \
	do { task_thread_info(lnxtsk)->status |= TS_USEDFPU; } while(0)
#endif

#define set_lnxtsk_using_fpu(lnxtsk) \
	do { rtai_set_fpu_used(lnxtsk); } while(0)

#endif /* !_RTAI_ASM_X86_FPU_H */
