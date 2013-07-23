/*
 * Copyright (C) 1999-2003 Paolo Mantegazza  <mantegazza@aero.polimi.it>
 * Copyright (C) 2000      Stuart Hughes     <shughes@zentropix.com>
 * Copyright (C) 2007      Antonio Barbalace <barbalace@igi.cnr.it>
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

#ifndef _RTAI_ASM_X86_64_SCHED_H
#define _RTAI_ASM_X86_64_SCHED_H

#define rt_exchange_tasks(oldtask, newtask) \
	__asm__ __volatile__( \
	"subq $9*8, %%rsp\n\t" \
	"movq %%rdi, 8*8(%%rsp)\n\t" \
	"movq %%rsi, 7*8(%%rsp)\n\t" \
	"movq %%rdx, 6*8(%%rsp)\n\t" \
	"movq %%rcx, 5*8(%%rsp)\n\t" \
	"movq %%rax, 4*8(%%rsp)\n\t" \
	"movq %%r8, 3*8(%%rsp)\n\t" \
	"movq %%r9, 2*8(%%rsp)\n\t" \
	"movq %%r10, 1*8(%%rsp)\n\t" \
	"movq %%r11, 0*8(%%rsp)\n\t" \
\
	"subq $6*8, %%rsp\n\t" \
	"movq %%rbx, 5*8(%%rsp)\n\t" \
	"movq %%rbp, 4*8(%%rsp)\n\t" \
	"movq %%r12, 3*8(%%rsp)\n\t" \
	"movq %%r13, 2*8(%%rsp)\n\t" \
	"movq %%r14, 1*8(%%rsp)\n\t" \
	"movq %%r15, 0*8(%%rsp)\n\t" \
\
	"pushq $1f\n\t" \
\
	"movq (%%rcx), %%rbx\n\t" \
	"movq %%rsp, (%%rbx)\n\t" \
	"movq (%%rdx), %%rsp\n\t" \
	"movq %%rdx, (%%rcx)\n\t" \
	"ret\n\t" \
\
"1:	 movq 0*8(%%rsp), %%r15\n\t" \
	"movq 1*8(%%rsp), %%r14\n\t" \
	"movq 2*8(%%rsp), %%r13\n\t" \
	"movq 3*8(%%rsp), %%r12\n\t" \
	"movq 4*8(%%rsp), %%rbp\n\t" \
	"movq 5*8(%%rsp), %%rbx\n\t" \
	"addq $6*8, %%rsp\n\t" \
\
	"movq 0*8(%%rsp), %%r11\n\t" \
	"movq 1*8(%%rsp), %%r10\n\t" \
	"movq 2*8(%%rsp), %%r9\n\t" \
	"movq 3*8(%%rsp), %%r8\n\t" \
	"movq 4*8(%%rsp), %%rax\n\t" \
	"movq 5*8(%%rsp), %%rcx\n\t" \
	"movq 6*8(%%rsp), %%rdx\n\t" \
	"movq 7*8(%%rsp), %%rsi\n\t" \
	"movq 8*8(%%rsp), %%rdi\n\t" \
	"addq $9*8, %%rsp\n\t" \
\
	: \
	: "c" (&oldtask), "d" (newtask) \
	);

#define init_arch_stack() \
do { \
	*--(task->stack) = data;		\
	*--(task->stack) = (unsigned long) rt_thread;	\
	*--(task->stack) = 0;			\
	*--(task->stack) = (unsigned long) rt_startup;	\
} while(0)

#define DEFINE_LINUX_CR0      static unsigned long linux_cr0;

#define DEFINE_LINUX_SMP_CR0  static unsigned long linux_smp_cr0[NR_RT_CPUS];

#define init_task_fpenv(task)  do { init_fpenv((task)->fpu_reg); } while(0)

static inline void *get_stack_pointer(void)
{
	void *sp;
	asm volatile ("movq %%rsp, %0" : "=r" (sp));
	return sp;
}

#define RT_SET_RTAI_TRAP_HANDLER(x)  rt_set_rtai_trap_handler(x)

#define DO_TIMER_PROPER_OP()

#endif /* !_RTAI_ASM_X86_64_SCHED_H */
