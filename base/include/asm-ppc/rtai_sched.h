/*
COPYRIGHT (C) 2000-2007  Paolo Mantegazza (mantegazza@aero.polimi.it)
	      2000       Stuart Hughes    (shughes@zentropix.com)

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


#ifndef RTAI_SCHED_H
#define RTAI_SCHED_H

#define	RTAI_MSR_FLAGS	(MSR_KERNEL | MSR_FP)

extern void up_task_sw(void *, void *);
#define rt_exchange_tasks(oldtask, newtask)  up_task_sw(&(oldtask), (new_task))

#define init_arch_stack() \
	do { \
		*(task->stack - 28) = data;		\
		*(task->stack - 29) = (long)rt_thread;	\
		*(task->stack - 35) = (long)rt_startup;	\
		*(task->stack - 36) = RTAI_MSR_FLAGS;	\
	} while(0)

#define DEFINE_LINUX_CR0      static unsigned long linux_cr0;

#define DEFINE_LINUX_SMP_CR0  static unsigned long linux_smp_cr0[NR_RT_CPUS];

#ifdef CONFIG_RTAI_FPU_SUPPORT
#define init_task_fpenv(task) \
	do { \
		memset(&task->fpu_reg, 0, sizeof(task->fpu_reg)); \
	} while(0)
#else
#define init_task_fpenv(task)  do { } while(0)
#endif

static inline void *get_stack_pointer(void)
{
	void *sp;
	asm volatile ("mr 1, %0" : "=r" (sp));
	return sp;
}

#define RT_SET_RTAI_TRAP_HANDLER(x)

#define DO_TIMER_PROPER_OP()

#endif
