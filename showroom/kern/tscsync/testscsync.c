/*
COPYRIGHT (C) 2006  Paolo Mantegazza <mantegazza@aero.polimi.it>

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


/*
	Hacked from arch/ia64/kernel/smpboot.c.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");

static volatile int tune;
static volatile long readofst;
static volatile long ofst[NR_CPUS];

static inline long long readtsc(void)
{
	long long t;
	__asm__ __volatile__("rdtsc" : "=A" (t));
	return tune ? t - ofst[hard_smp_processor_id()] : t;
}

#define MASTER	(0)
#define SLAVE	(SMP_CACHE_BYTES/8)

#define NUM_ITERS  10

static DEFINE_SPINLOCK(tsc_sync_lock);
static DEFINE_SPINLOCK(tsclock);
static volatile long long go[SLAVE + 1];

static void sync_master(void *arg)
{
	unsigned long flags, lflags, i;

	if ((unsigned long)arg != hard_smp_processor_id()) {
		return;
	}

	go[MASTER] = 0;
	local_irq_save(flags);
	for (i = 0; i < NUM_ITERS; ++i) {
		while (!go[MASTER]) {
			cpu_relax();
		}
		go[MASTER] = 0;
		spin_lock_irqsave(&tsclock, lflags);
		go[SLAVE] = readtsc();
		spin_unlock_irqrestore(&tsclock, lflags);
	}
	local_irq_restore(flags);
}

static inline long long get_delta (long long *rt, long long *master, unsigned int slave)
{
	unsigned long long best_t0 = 0, best_t1 = ~0ULL, best_tm = 0;
	unsigned long long tcenter, t0, t1, tm;
	long i, lflags;

	for (i = 0; i < NUM_ITERS; ++i) {
		t0 = readtsc();
		go[MASTER] = 1;
		spin_lock_irqsave(&tsclock, lflags);
		while (!(tm = go[SLAVE])) {
			spin_unlock_irqrestore(&tsclock, lflags);
			cpu_relax();
			spin_lock_irqsave(&tsclock, lflags);
		}
		spin_unlock_irqrestore(&tsclock, lflags);
		go[SLAVE] = 0;
		t1 = readtsc();

		if (t1 - t0 < best_t1 - best_t0) {
			best_t0 = t0, best_t1 = t1, best_tm = tm;
		}
	}

	*rt = best_t1 - best_t0;
	*master = best_tm - best_t0;

	/* average best_t0 and best_t1 without overflow: */
	tcenter = (best_t0/2 + best_t1/2);
	if (best_t0 % 2 + best_t1 % 2 == 2) {
		++tcenter;
	}
	if (tune) {
		return tcenter - best_tm;
	} else {
		return ofst[slave] = (8*ofst[slave] + 2*((long)(tcenter - best_tm)))/10;
	}
}

void rtai_sync_tsc (unsigned int master, unsigned int slave)
{
	unsigned long flags;
	long long delta, rt, master_time_stamp;

	go[MASTER] = 1;

	if (smp_call_function(sync_master, (void *)master, 1, 0) < 0) {
		printk(KERN_ERR "sync_tsc: failed to get attention of CPU %u!\n", master);
		return;
	}

	while (go[MASTER]) {
		cpu_relax();	/* wait for master to be ready */
	}

	spin_lock_irqsave(&tsc_sync_lock, flags);
	delta = get_delta(&rt, &master_time_stamp, slave);
	spin_unlock_irqrestore(&tsc_sync_lock, flags);

	printk(KERN_INFO "CPU %u: synced its TSC with CPU %u (master time stamp %llu cycles, < - OFFSET %lld cycles - > , max double tsc read span %llu cycles)\n", slave, master, master_time_stamp, delta, rt);
}

#define MASTER_CPU  0
#define SLEEP       1000 // ms
static volatile int end;

static void kthread_fun(void *null)
{
	int i = 0, k;
	printk("*** MASTER CPU %d (TIME TO READ TSC %ld) ***\n", first_cpu(current->cpus_allowed), readofst);
	while (!end) {
		printk(KERN_INFO "Loop %d:\n", ++i);
		for (k = 0; k < num_online_cpus(); k++) {
			tune = 1;
			if (k != MASTER_CPU) {
				set_cpus_allowed(current, cpumask_of_cpu(k));
				rtai_sync_tsc(MASTER_CPU, k);
			}
			tune = 0;
			if (k != MASTER_CPU) {
				rtai_sync_tsc(MASTER_CPU, k);
			}
		}
		msleep(SLEEP);
	}
}

#define NTERMS 100
int init_module(void)
{
	long long t = 0, tt;
	int i;
	t = readtsc();
	for (i = 0; i < NTERMS; i++) {
		 tt = readtsc();
	}
	readofst = ((long)(readtsc() - t))/NTERMS;
	kernel_thread((void *)kthread_fun, NULL, 0);
	return 0;
}

void cleanup_module(void)
{
	end = 1;
	msleep(SLEEP);
	return;
}
