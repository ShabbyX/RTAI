/*
 * Copyright (C) 2006 Paolo Mantegazza <mantegazza@aero.polimi.it>.
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

#include <rtai.h>
#include <asm/rtai_lxrt.h>

/*+++++++++++++++++++++ OUR LITTLE STAND ALONE LIBRARY +++++++++++++++++++++++*/

#ifndef STR
#define __STR(x) #x
#define STR(x) __STR(x)
#endif

#ifndef SYMBOL_NAME_STR
#define SYMBOL_NAME_STR(X) #X
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,20)
#define __PUSH_FGS          "pushl %fs\n\t"
#define __POP_FGS           "popl  %fs\n\t"
#define __LOAD_KERNEL_PDA  "movl  $"STR(__KERNEL_PERCPU)",%edx; movl %edx,%fs\n\t"
#elif LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19)
#define __PUSH_FGS          "pushl %gs\n\t"
#define __POP_FGS           "popl  %gs\n\t"
#define __LOAD_KERNEL_PDA  "movl  $"STR(__KERNEL_PDA)",%edx; movl %edx,%gs\n\t"
#else
#define __PUSH_FGS
#define __POP_FGS
#define __LOAD_KERNEL_PDA
#endif

#define DEFINE_VECTORED_ISR(name, fun) \
	__asm__ ( \
        "\n" __ALIGN_STR"\n\t" \
        SYMBOL_NAME_STR(name) ":\n\t" \
	"pushl $0\n\t" \
	"cld\n\t" \
        __PUSH_FGS \
        "pushl %es\n\t" \
        "pushl %ds\n\t" \
        "pushl %eax\n\t" \
        "pushl %ebp\n\t" \
	"pushl %edi\n\t" \
        "pushl %esi\n\t" \
        "pushl %edx\n\t" \
        "pushl %ecx\n\t" \
	"pushl %ebx\n\t" \
	__LXRT_GET_DATASEG(edx) \
        "movl %edx, %ds\n\t" \
        "movl %edx, %es\n\t" \
        __LOAD_KERNEL_PDA \
        "call "SYMBOL_NAME_STR(fun)"\n\t" \
        "testl %eax,%eax\n\t" \
        "jnz  ret_from_intr\n\t" \
        "popl %ebx\n\t" \
        "popl %ecx\n\t" \
        "popl %edx\n\t" \
        "popl %esi\n\t" \
	"popl %edi\n\t" \
        "popl %ebp\n\t" \
        "popl %eax\n\t" \
        "popl %ds\n\t" \
        "popl %es\n\t" \
        __POP_FGS \
	"addl $4, %esp\n\t" \
        "iret")

struct desc_struct rtai_set_gate_vector (unsigned vector, int type, int dpl, void *handler)
{
	struct desc_struct e = idt_table[vector];
	idt_table[vector].a = (__KERNEL_CS << 16) | ((unsigned)handler & 0x0000FFFF);
	idt_table[vector].b = ((unsigned)handler & 0xFFFF0000) | (0x8000 + (dpl << 13) + (type << 8));
	return e;
}

void rtai_reset_gate_vector (unsigned vector, struct desc_struct e)
{
	idt_table[vector] = e;
}

/*++++++++++++++++++ END OF OUR LITTLE STAND ALONE LIBRARY +++++++++++++++++++*/

#define MAX_RTC_FREQ  8192
#define RTC_FREQ      MAX_RTC_FREQ

int PERIOD = (1000000000 + RTC_FREQ/2)/RTC_FREQ; // nanos
RTAI_MODULE_PARM(PERIOD, int);

int ECHO_PERIOD = 1000; // ms
RTAI_MODULE_PARM(ECHO_PERIOD, int);

#define RTC_IRQ  8
#define RTC_VEC  (ext_irq_vector(RTC_IRQ))

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

static inline unsigned char CMOS_READ(unsigned char addr)
{
        outb((addr),RTC_PORT(0));
        pause_io();
        return inb(RTC_PORT(1));
}

#define CMOS_WRITE(val, addr) ({ \
	outb((addr),RTC_PORT(0)); \
	pause_io(); \
	outb((val),RTC_PORT(1)); \
	pause_io(); \
})

static RTIME t0;
static int tsc_period, maxj, maxj_echo, pasd;

void rtc_handler (int irq, unsigned long rtc_freq)
{
        static int stp, cnt;
        if (++cnt == rtc_freq) {
                rt_printk("<> IRQ %d, %d: CNT %d <>\n", irq, ++stp, cnt);
                cnt = 0;
        }

	if (pasd) {
		RTIME t;
		int jit;

		t = rdtsc();
		if ((jit = abs((int)(t - t0) - tsc_period)) > maxj) {
			maxj = jit;
		}
		t0 = t;
	} else {
		pasd = 1;
		t0 = rdtsc();
	}
 	CMOS_READ(RTC_INTR_FLAGS);
	rt_enable_irq(RTC_IRQ);
}

void asm_handler(void);
DEFINE_VECTORED_ISR(asm_handler, rtc_handler);

static struct desc_struct desc;

#define MIN_RTC_FREQ  2

static void rtc_start(long rtc_freq)
{
	int pwr2;
	unsigned long flags;

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

	rt_disable_irq(RTC_IRQ);
	flags = hal_critical_enter(NULL);
	desc = rtai_set_gate_vector(RTC_VEC, 14, 0, asm_handler);
	hal_critical_exit(flags);
	rtai_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
	CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	CMOS_WRITE(RTC_REF_CLCK_32KHZ | (16 - pwr2),          RTC_FREQ_SELECT);
	CMOS_WRITE((CMOS_READ(RTC_CONTROL) & 0x8F) | RTC_PIE, RTC_CONTROL);
	rt_enable_irq(RTC_IRQ);
	CMOS_READ(RTC_INTR_FLAGS);
	rtai_sti();
}

static void rtc_stop(void)
{
	unsigned long flags;

	rt_disable_irq(RTC_IRQ);
	flags = hal_critical_enter(NULL);
	rtai_reset_gate_vector(RTC_VEC, desc);
	hal_critical_exit(flags);
	rtai_cli();
	CMOS_WRITE(CMOS_READ(RTC_FREQ_SELECT), RTC_FREQ_SELECT);
	CMOS_WRITE(CMOS_READ(RTC_CONTROL),     RTC_CONTROL);
	rtai_sti();
}

static struct timer_list timer;

static void timer_fun(unsigned long none)
{
	int t;
	if (maxj_echo < maxj) {
		maxj_echo = maxj;
		t = imuldiv(maxj_echo, 1000000000, rtai_tunables.cpu_freq);
		printk("INCREASED TO: %d.%-3d (us)\n", t/1000, t%1000); // silly and wrong but acceptable
	}
	mod_timer(&timer, jiffies + ECHO_PERIOD*HZ/1000);
}

int _init_module(void)
{
	init_timer(&timer);
	timer.function = timer_fun;
	mod_timer(&timer, jiffies + ECHO_PERIOD*HZ/1000);
	printk("\nCHECKING WITH PERIOD: %d (us)\n\n", PERIOD/1000);
	tsc_period = imuldiv(PERIOD, rtai_tunables.cpu_freq, 1000000000);
	rtc_start(RTC_FREQ);
	return 0;
}

void _cleanup_module(void)
{
	int t;

	rtc_stop();
	del_timer(&timer);
	t = imuldiv(maxj_echo, 1000000000, rtai_tunables.cpu_freq);
	printk("\nCHECKED WITH PERIOD: %d (us), MAXJ: %d.%-3d (us)\n\n", PERIOD/1000, t/1000, t%1000);
	return;
}

module_init(_init_module);
module_exit(_cleanup_module);
