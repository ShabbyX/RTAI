/*
 * Original copyright:
 *
 * This kernel module has been developed to provide an RTAI interface
 * for the timers on the VMIC 7700 board. The file I/O nature of the
 * VMIC driver has been replaced by a suite of calls that can be made
 * from other kernel-space modules.
 *
 * The PCI initialization mechanism has been retained to minimize the
 * the changes in this module.
 *
 * Copyright 2004 Akamina Technologies Inc
 *
 *
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2002 VMIC
    International Copyright Secured.  All Rights Reserved.

-------------------------------------------------------------------------------
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   o Redistributions of source code must retain the above copyright notice, this     list of conditions and the following disclaimer.
   o Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   o Neither the name of VMIC nor the names of its contributors may be used to
     endorse or promote products derived from this software without specific
     prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

===============================================================================
 *
 * This is an extensive rewrite for carrying out the same test on a generic
 * PC, by Paolo Mantegazza (mantegazza@aero.polimi.it).
 *
 */

#include <linux/module.h>
#include <asm/uaccess.h>

MODULE_LICENSE("GPL");

#include <rtai_sem.h>
#include "lxrtext.h"

static unsigned long timer_irq_map, timer_on;

static int timer_isr_count[NUM_TIMERS];

static SEM tmr_sema[NUM_TIMERS];

static void tmr_get_setup(int timer, int *period, int *freq)
{
	*period = LATCH;
	*freq   = CLOCK_TICK_RATE;
}

static void tmr_start(int timer)
{
	set_bit(timer, &timer_on);
}

static void tmr_stop(int timer)
{
	clear_bit(timer, &timer_on);
}

static int tmr_get_isr_count(int timer)
{
	return timer_isr_count[timer];
}

static inline int _timer_get_count(int timer)
{
	int count;
	outb(0x00, 0x43);
	count = inb(0x40);
	return count | (inb(0x40) << 8);
}

static RTIME tirq, thandler, t8254, t2task;
static int cntirq, cnt8254, cnt2task;

static int tmr_get_count(int timer, RTIME *cputime)
{
	RTIME t;
	unsigned long flags;
	int count;

	t = rdtsc();
	hal_hw_local_irq_save(flags);
	count = _timer_get_count(timer);
	hal_hw_local_irq_restore(flags);
	*cputime = rt_get_cpu_time_ns();
	t8254 += (rdtsc() - t);
	cnt8254++;
	return count;
}

static int wait_on_timer_ext(int timer, int *count, RTIME *cputime)
{
	RTIME t;
	unsigned long flags;
	int semcnt;

	semcnt = rt_sem_wait(&tmr_sema[timer]);
	t = rdtsc();
	hal_hw_local_irq_save(flags);
	*count = _timer_get_count(timer);
	hal_hw_local_irq_restore(flags);
	*cputime = rt_get_cpu_time_ns();
	t8254 += (rdtsc() - t);
	cnt8254++;
	t2task += (t - tirq);
	cnt2task++;
	return semcnt;
}

static int wait_on_timer(int timer)
{
	int semcnt;
	semcnt = rt_sem_wait(&tmr_sema[timer]);
	t2task += (rdtsc() - tirq);
	cnt2task++;
	return semcnt;
}

static inline int is_irq_from(int timer)
{
	return test_and_clear_bit(timer, &timer_irq_map) && test_bit(timer, &timer_on) ? timer : 0;
}

static void tmr_irq_handler(int irq)
{
	int timer;
	tirq = rdtsc();
	set_bit(MY_TIMER, &timer_irq_map);  // a kinda silly emulation
	for (timer = 0; timer < NUM_TIMERS; timer++) {
		if (is_irq_from(timer)) {
			rtai_cli();  // to be safe whatever ADEOS does
			timer_isr_count[timer] = _timer_get_count(timer);
//			rt_ack_irq(irq);  // not needed, just a latency maker
// the line below is wrong, we want to check overruns by using a counting sem
//			rt_sem_wait_if(&tmr_sema[timer]);
			thandler += (rdtsc() - tirq);
			cntirq++;
			rt_sem_signal(&tmr_sema[timer]);	
		}
	}
	rt_pend_linux_irq(irq);
}

#define NON_RT_BLOCKING 0
#define RT_BLOCKING 1

static struct rt_fun_entry rt_proxy[] = {
	[TMR_GET_SETUP]     = { NON_RT_BLOCKING, tmr_get_setup },
	[TMR_START]         = { NON_RT_BLOCKING, tmr_start },
	[TMR_STOP]          = { NON_RT_BLOCKING, tmr_stop },
	[TMR_GET_ISR_COUNT] = { NON_RT_BLOCKING, tmr_get_isr_count },
	[TMR_GET_COUNT]     = { NON_RT_BLOCKING, tmr_get_count },
	[WAIT_ON_TIMER_EXT] = { RT_BLOCKING,     wait_on_timer_ext },
	[WAIT_ON_TIMER]     = { RT_BLOCKING,     wait_on_timer }
};

int init_rtai_proxy( void )
{
	if(set_rt_fun_ext_index(rt_proxy, TMR_INDX)) {
		printk("Recompile your module with a different index\n");
		return -EACCES;
	}
// we use a counting sem to check for all possible overruns
//	rt_typed_sem_init(&tmr_sema[MY_TIMER], 0, BIN_SEM);
	rt_typed_sem_init(&tmr_sema[MY_TIMER], 0, CNT_SEM);
	rt_assign_irq_to_cpu(TIMER_IRQ, ALLOWED_CPUS);
	rt_request_global_irq(TIMER_IRQ, (void *)tmr_irq_handler);
	return 0;
}

void cleanup_rtai_proxy( void )
{
	rt_free_global_irq(TIMER_IRQ);
	rt_reset_irq_to_sym_mode(TIMER_IRQ);
	rt_sem_delete(&tmr_sema[MY_TIMER]);
	reset_rt_fun_ext_index(rt_proxy, TMR_INDX);
	t8254 = llimd(t8254 + cnt8254/2, 1, cnt8254 + 1);
	printk("AVERAGE TIME TO READ TIMER: %d (ns).\n", imuldiv((int)t8254, 1000000000, rtai_tunables.cpu_freq));
	thandler = llimd(thandler + cntirq/2, 1, cntirq + 1);
	printk("AVERAGE TIME FROM IRQ TO SEM SIGNAL: %d (ns).\n", imuldiv((int)thandler, 1000000000, rtai_tunables.cpu_freq));
	t2task = llimd(t2task + cnt2task/2, 1, cnt2task + 1);
	printk("AVERAGE TIME FROM IRQ TO TASK: %d (ns).\n", imuldiv((int)t2task, 1000000000, rtai_tunables.cpu_freq));
}

module_init(init_rtai_proxy);
module_exit(cleanup_rtai_proxy);
