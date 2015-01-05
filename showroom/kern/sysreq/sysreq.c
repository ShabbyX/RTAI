/*
COPYRIGHT (C) 1999-2013 Paolo Mantegazza (mantegazza@aero.polimi.it)

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


#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/semaphore.h>
#include <asm/uaccess.h>

#include <asm/rtai.h>
#include <rtai_wrappers.h>

MODULE_LICENSE("GPL");

#define TIMER_FREQ 1000 // !!! 0 < TIMER_FREQ < HZ !!!

static int srq, scount;

static DECLARE_MUTEX_LOCKED(sem);

static long long user_srq_handler(unsigned long whatever)
{
	long long time;

	++scount;

	if (whatever == 1) {
		return (long long)((TIMER_FREQ*1000)/HZ);
	}

	if (whatever == 2) {
		return (long long)scount;
	}

	down_interruptible(&sem);

// let's show how to communicate. Copy to and from user shall allow any kind of
// data interchange and service.
	time = llimd(rtai_rdtsc(), 1000000, CPU_FREQ);
	copy_to_user((long long *)whatever, &time, sizeof(long long));
	return time;
}

static void rtai_srq_handler(void)
{
	up(&sem);
}

static struct timer_list timer;

static void rt_timer_handler(unsigned long none)
{
#if 0 // diagnose and see if interrupts are coming in
	static int cnt[NR_RT_CPUS];
	int cpuid = rtai_cpuid();
	rt_printk("TIMER TICK: CPU %d, %d\n", cpuid, ++cnt[cpuid]);
#endif
	rt_pend_linux_srq(srq);
	mod_timer(&timer, jiffies + (HZ/TIMER_FREQ));
	return;
}

int init_module(void)
{
	srq = rt_request_srq(0xcacca, rtai_srq_handler, user_srq_handler);
        init_timer(&timer);
        timer.function = rt_timer_handler;
	mod_timer(&timer, (HZ/TIMER_FREQ));
        return 0 ;
}

void cleanup_module(void)
{
        del_timer(&timer);
	rt_free_srq(srq);
}
