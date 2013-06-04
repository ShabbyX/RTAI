/*
 * Copyright (C) 1999-2003 Paolo Mantegazza <mantegazza@aero.polimi.it>
 * Copyright (C) 2000      Stuart Hughes    <shughes@zentropix.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _RTAI_ASM_M68KNOMMU_SCHED_H
#define _RTAI_ASM_M68KNOMMU_SCHED_H

#define rt_exchange_tasks(oldtask, newtask) \
       __asm__ __volatile__( \
           "lea %%sp@(-60),%%sp\n\t" \
           "movem.l %%d0-%%d7/%%a0-%%a6,%%sp@\n\t" \
           "pea %%pc@(1f)\n\t" \
           "move.l (%0),%%a1\n\t" \
           "move.l %%sp,(%%a1)\n\t" \
           "move.l %1,(%0)\n\t" \
           "move.l %1,%%a1\n\t" \
           "move.l (%%a1),%%sp\n\t" \
           "rts\n\t" \
           "1:  movem.l %%sp@,%%d0-%%d7/%%a0-%%a6\n\t" \
           "lea %%sp@(60),%%sp\n\t" \
           : /* no output */ \
           : "a" (&oldtask) , "d" (newtask) \
           : "%a1", "memory");

#define init_arch_stack() \
do { \
	*--(task->stack) = data;		\
	*--(task->stack) = (int) rt_thread;	\
	*--(task->stack) = 0;			\
	*--(task->stack) = (int) rt_startup;	\
} while(0)

#define DEFINE_LINUX_CR0

#define DEFINE_LINUX_SMP_CR0  static unsigned long linux_smp_cr0[NR_RT_CPUS];

#define init_task_fpenv(task)  do { init_fpenv((task)->fpu_reg); } while(0)

static inline void *get_stack_pointer(void)
{
	void *sp;
	asm volatile ("movl %%sp, %0" : "=d" (sp));
	return sp;
}

#define RT_SET_RTAI_TRAP_HANDLER(x)  rt_set_rtai_trap_handler(x)

#define DO_TIMER_PROPER_OP()

#endif /* !_RTAI_ASM_M68KNOMMU_SCHED_H */
