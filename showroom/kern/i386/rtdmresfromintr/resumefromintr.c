/*
 * COPYRIGHT (C) 2008  Paolo Mantegazza <mantegazza@aero.polimi.it>
 *
 * RTAI/fusion is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * RTAI/fusion is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RTAI/fusion; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/module.h>
#include <asm/io.h>

#include <rtai_sched.h>
#include <rtai_sem.h>
#include <rtai_msg.h>
#include <rtdm/rtdm_driver.h>

#define PERIOD  (1000000000 + RTC_FREQ/2)/RTC_FREQ // nanos
#define TASK_CPU  1
#define IRQ_CPU   1

#define STACK_SIZE 4000

#define CMDF 0

RT_TASK thread;

static int cpu_used[NR_RT_CPUS + 1];

int ECHO_PERIOD = 1000; // ms
RTAI_MODULE_PARM(ECHO_PERIOD, int);

#define RTC_IRQ  8

#define MAX_RTC_FREQ  8192
#define RTC_FREQ      MAX_RTC_FREQ

#define RTC_REG_A  10
#define RTC_REG_B  11
#define RTC_REG_C  12

#define RTC_CONTROL      RTC_REG_B
#define RTC_INTR_FLAGS   RTC_REG_C
#define RTC_FREQ_SELECT  RTC_REG_A

#define RTC_REF_CLCK_32KHZ  0x20
#define RTC_PIE             0x40

#define RTC_PORT(x)     (0x70 + (x))
#define RTC_ALWAYS_BCD  1

#define pause_io() \
	do { asm volatile("outb %%al,$0x80" : : : "memory"); } while (0)

#define CMOS_READ(addr) ({ \
	outb((addr),RTC_PORT(0)); \
	pause_io(); \
	inb(RTC_PORT(1)); \
})

#define CMOS_WRITE(val, addr) ({ \
	outb((addr),RTC_PORT(0)); \
	pause_io(); \
	outb((val),RTC_PORT(1)); \
	pause_io(); \
})

static int go;

static int rtc_handler(rtdm_irq_t *none)
{
 	CMOS_READ(RTC_INTR_FLAGS);
	if (go) {
		rt_task_resume(&thread);
	}
	return 0;
}

rtdm_irq_t irqh;

#define MIN_RTC_FREQ  2

static void rtc_start(long rtc_freq)
{
	int pwr2;

	if (rtc_freq <= 0) {
		rtc_freq = RTC_FREQ;
	}
	if (rtc_freq > MAX_RTC_FREQ) {
		rtc_freq = MAX_RTC_FREQ;
	} else if (rtc_freq < MIN_RTC_FREQ) {
		rtc_freq = MIN_RTC_FREQ;
	}
	pwr2 = 1;
	if (rtc_freq > MIN_RTC_FREQ) {
		while (rtc_freq > (1 << pwr2)) {
			pwr2++;
		}
		if (rtc_freq <= ((3*(1 << (pwr2 - 1)) + 1)>>1)) {
			pwr2--;
		}
	}

	rtdm_irq_request(&irqh, RTC_IRQ, rtc_handler, 0, "RTC", NULL);
	rtai_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
	CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	CMOS_WRITE(RTC_REF_CLCK_32KHZ | (16 - pwr2),          RTC_FREQ_SELECT);
	CMOS_WRITE((CMOS_READ(RTC_CONTROL) & 0x8F) | RTC_PIE, RTC_CONTROL);
	CMOS_READ(RTC_INTR_FLAGS);
	rtai_sti();
}

static void rtc_stop(void)
{
	rtdm_irq_free(&irqh);
	rtai_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
	CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	rtai_sti();
}

static void thread_fun(long dummy)
{
	RT_TASK *task;
	RTIME t0 = 0, t;
	long count = RTC_FREQ, jit, maxj = 0, echo = 0;

	task = rt_receive(NULL, (unsigned long *)&jit);
	if (task != (void *)jit) {
		rt_printk("INCONSISTENT RECEIVE %p %p\n", task, (void *)jit);
		return;
	}
	rt_return(task, 0UL);
	go = 1;

	while(!rt_task_suspend(&thread)) {
		cpu_used[hard_cpu_id()]++;
		if (!count) {
			t = rt_get_cpu_time_ns();
			if ((jit = t - t0 - PERIOD) < 0) {
				jit = -jit;
			}
			if (jit > maxj) {
				maxj = jit;
			}
			t0 = t;
			if (echo++ > RTC_FREQ/10) {
				echo = 0;
				rt_send_if(task, maxj);
			}
		} else {
			t0 = rt_get_cpu_time_ns();
			count--;
		}
	}
}


int _init_module(void)
{
	rt_assign_irq_to_cpu(RTC_IRQ, (1 << IRQ_CPU));
	rt_task_init_cpuid(&thread, thread_fun, 0, STACK_SIZE, 0, 0, 0, TASK_CPU);
	rt_register(nam2num("RPCTSK"), &thread, IS_TASK, 0);
	rt_task_resume(&thread);
	rtc_start(RTC_FREQ);
	return 0;
}


void _cleanup_module(void)
{
	int cpuid;
	rtc_stop();
	rt_reset_irq_to_sym_mode(RTC_IRQ);
	rt_drg_on_name(nam2num("RPCTSK"));
	rt_task_delete(&thread);
	printk("\n\nCPU USE SUMMARY\n");
	for (cpuid = 0; cpuid < NR_RT_CPUS; cpuid++) {
		printk("# %d -> %d\n", cpuid, cpu_used[cpuid]);
	}
	printk("# ints -> %d\n",  cpu_used[NR_RT_CPUS]);
	printk("END OF CPU USE SUMMARY\n\n");
}

module_init(_init_module);
module_exit(_cleanup_module);
