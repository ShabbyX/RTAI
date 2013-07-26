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

#define __PUSH_GS          "pushl %gs\n\t"
#define __POP_GS           "popl  %gs\n\t"
#define __LOAD_KERNEL_PDA  "movl  $"STR(__KERNEL_PDA)",%edx; movl %edx,%gs\n\t"

static void asm_handler (void)
{
    __asm__ __volatile__ ( \
	"cld\n\t" \
	__PUSH_GS \
	"pushl %es\n\t" \
	"pushl %ds\n\t" \
	"pushl %eax\n\t" \
	"pushl %ebp\n\t" \
	"pushl %edi\n\t" \
	"pushl %esi\n\t" \
	"pushl %edx\n\t" \
	"pushl %ecx\n\t" \
	"pushl %ebx\n\t" \
	__LXRT_GET_DATASEG(ebx) \
	"movl %ebx, %ds\n\t" \
	"movl %ebx, %es\n\t" \
	__LOAD_KERNEL_PDA \
	"call "SYMBOL_NAME_STR(c_handler)"\n\t" \
	"popl %ebx\n\t" \
	"popl %ecx\n\t" \
	"popl %edx\n\t" \
	"popl %esi\n\t" \
	"popl %edi\n\t" \
	"popl %ebp\n\t" \
	"popl %eax\n\t" \
	"popl %ds\n\t" \
	"popl %es\n\t" \
	__POP_GS \
	"iret");
}

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

#define IRQ 0

#ifdef CONFIG_SMP
static int vector = 0x31; // SMPwise it is likely 0x31
#else
static int vector = 0x20; //  UPwise it is likely 0x20
#endif

static unsigned long cr0;
union i387_union saved_fpu_reg, my_fpu_reg;
static volatile float fcnt = 0.0;
static volatile int cnt;

void c_handler (void)
{
	hal_root_domain->irqs[IRQ].acknowledge(IRQ);
	save_fpcr_and_enable_fpu(cr0);
	save_fpenv(saved_fpu_reg);
	restore_fpenv(my_fpu_reg);
	++cnt;
	++fcnt;
	save_fpenv(my_fpu_reg);
	restore_fpenv(saved_fpu_reg);
	restore_fpcr(cr0);
	rt_pend_linux_irq(IRQ);
}

static struct desc_struct desc;

#define ECHO_PERIOD 1
static struct timer_list timer;

static void timer_fun(unsigned long none)
{
	printk("HEY HERE I AM, INTERRUPTS COUNT %d\n", cnt);
	mod_timer(&timer, jiffies + ECHO_PERIOD*HZ);
}

int _init_module(void)
{
	unsigned long flags;
	init_timer(&timer);
	timer.function = timer_fun;
	mod_timer(&timer, jiffies + ECHO_PERIOD*HZ);
	printk("TIMER IRQ/VECTOR %d/%d\n", IRQ, vector);
	save_fpcr_and_enable_fpu(cr0);
	save_fpenv(my_fpu_reg);
	restore_fpcr(cr0);
	flags = hal_critical_enter(NULL);
	desc = rtai_set_gate_vector(vector, 14, 0, asm_handler);
	hal_critical_exit(flags);
	return 0;
}

void _cleanup_module(void)
{
	unsigned long flags;
	del_timer(&timer);
	flags = hal_critical_enter(NULL);
	rtai_reset_gate_vector(vector, desc);
	hal_critical_exit(flags);
	printk("TIMER IRQ/VECTOR %d/%d, FPCOUNT %lu\n", IRQ, vector, (unsigned long)fcnt);
	return;
}

module_init(_init_module);
module_exit(_cleanup_module);
