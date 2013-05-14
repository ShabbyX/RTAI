/*
COPYRIGHT (C) 2006  Paolo Mantegazza (mantegazza@aero.polimi.it)

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

#include <rtai.h>
#include <rtai_schedcore.h>
#include <rtai_fifos.h>
#include <rtai_scb.h>
#include <rtai_sched.h>
#include <rtai_nam2num.h>

MODULE_LICENSE("GPL");

/* RTAI FROM LINUX SOFT KERNEL THREAD, TAKEN VERBATIM FROM NETRPC.C */

/*
THIS IS A BIT CRYPTIC AND REQUIRES SOME CARE, QUITE GENERAL THOUGH.
MAKING IT EASIER TO USE IS JUST A MATTER OF APPROPRIATE WRAPPERS.
NONE HAS SHOWN INTEREST IN SUCH A THING, SO LET"S KEEP THE TRUE MAGIC
FOR THE TRUE RTAI MAGICIANS ONLY.
*/

int get_min_tasks_cpuid(void);
int set_rtext(RT_TASK *, int, int, void(*)(void), unsigned int, void *);
int clr_rtext(RT_TASK *);
void rt_schedule_soft(RT_TASK *);

static inline int soft_rt_fun_call(RT_TASK *task, void *fun, void *arg)
{
	task->fun_args[0] = (long)arg;
	((struct fun_args *)task->fun_args)->fun = fun;
	rt_schedule_soft(task);
	return task->retval;
}

static inline long long soft_rt_genfun_call(RT_TASK *task, void *fun, void *args, int argsize)
{
	memcpy(task->fun_args, args, argsize);
	((struct fun_args *)task->fun_args)->fun = fun;
	task->retval = 0;
	rt_schedule_soft(task);
	return task->retval;
}

static void thread_fun(RT_TASK *task)
{
	if (!set_rtext(task, task->fun_args[3], 0, 0, get_min_tasks_cpuid(), 0)) {
		int lnxprio;
		if ((lnxprio = 99 - task->fun_args[3])) {
			lnxprio = 1;
		}
		sigfillset(&current->blocked);
		rtai_set_linux_task_priority(current, SCHED_FIFO, lnxprio);
		soft_rt_fun_call(task, rt_task_suspend, task);
		((void (*)(long))task->fun_args[1])(task->fun_args[2]);
	}
}
static int soft_kthread_init(RT_TASK *task, long fun, long arg, int priority)
{
	task->magic = task->state = 0;
	(task->fun_args = (long *)(task + 1))[1] = fun;
	task->fun_args[2] = arg;
	task->fun_args[3] = priority;
	if (kernel_thread((void *)thread_fun, task, 0) > 0) {
		while (task->state != (RT_SCHED_READY | RT_SCHED_SUSPENDED)) {
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(HZ/10);
		}
		return 0;
	}
	return -ENOEXEC;
}

static int soft_kthread_delete(RT_TASK *task)
{
	if (clr_rtext(task)) {
		return -EFAULT;
	} else {
		struct task_struct *lnxtsk = task->lnxtsk;
		lnxtsk->state = TASK_INTERRUPTIBLE;
		kill_proc(lnxtsk->pid, SIGTERM, 0);
	}
	return 0;
}

/* END OF RTAI FROM LINUX SOFT KERNEL THREAD */

#define LINUX_FIRST 1 // 0/1 toggle this to see RTAI magics

struct msg_s { char test[8]; int cnt; };

/*
try playing the game even with BUFSIZ < sizeof(struct msg_s), down to a
single byte buffer. It's fun, not effcient though.
*/
#define BUFSIZ (10*sizeof(struct msg_s))

static RT_TASK rtai_task, *linux_task;
static struct task_struct *linux_kthread;

static SEM rtai_sem, linux_sem;
static MBX mbx;

#if LINUX_FIRST  // wake up linux first

static irqreturn_t handler(int irq, void *dev_id, struct pt_regs *regs)
{
	rt_sem_signal(&linux_sem);  // no care, it is non blocking
	return RTAI_LINUX_IRQ_HANDLED;
}

static void rtai_fun(long t)
{
	struct msg_s tstfifo = { "TSTCNT:", 0 };
	while (1) {
		rt_sem_wait(&rtai_sem);
		tstfifo.cnt++;
		if (!rt_mbx_send(&mbx, &tstfifo, sizeof(tstfifo))) {
			rt_printk("RTAI  > %s %d\n", tstfifo.test, tstfifo.cnt);
		}
	}
}

static void linux_fun(long t)
{
	struct msg_s tstfifo;
	struct { MBX *mbx; void *msg; long msgsiz; } mbxrcv = { &mbx, &tstfifo, sizeof(tstfifo) };

	linux_kthread = current;
	while (1) {
		soft_rt_fun_call(linux_task, rt_sem_wait, &linux_sem);
		rt_sem_signal(&rtai_sem); // no care, it is non blocking
		if (!(int)soft_rt_genfun_call(linux_task, rt_mbx_receive, &mbxrcv, sizeof(mbxrcv))) {
			printk("LINUX > %s %d\n", tstfifo.test, tstfifo.cnt);
		}
	}
}

#else  // wake up rtai first

static irqreturn_t handler(int irq, void *dev_id, struct pt_regs *regs)
{
	rt_sem_signal(&rtai_sem);
	return RTAI_LINUX_IRQ_HANDLED;
}

static void rtai_fun(long t)
{
	struct msg_s tstfifo = { "TSTCNT:", 0 };
	while (1) {
		rt_sem_wait(&rtai_sem);
		tstfifo.cnt++;
		if (!rt_mbx_send(&mbx, &tstfifo, sizeof(tstfifo))) {
			rt_printk("RTAI  > %s %d\n", tstfifo.test, tstfifo.cnt);
		}
	}
}

static void linux_fun(long t)
{
	struct msg_s tstfifo;
	struct { MBX *mbx; void *msg; long msgsiz; } mbxrcv = { &mbx, &tstfifo, sizeof(tstfifo) };

	linux_kthread = current;
	while (1) {
		if (!(int)soft_rt_genfun_call(linux_task, rt_mbx_receive, &mbxrcv, sizeof(mbxrcv))) {
			printk("LINUX > %s %d\n", tstfifo.test, tstfifo.cnt);
		}
	}
}

#endif

int init_module(void)
{
	rt_sem_init(&rtai_sem, 0);
	rt_sem_init(&linux_sem, 0);
	rt_mbx_init(&mbx, BUFSIZ);
	rt_task_init(&rtai_task, rtai_fun, 0, 8000, 0, 0, 0);
	rt_task_resume(&rtai_task);
	linux_task = kmalloc(sizeof(RT_TASK) + 2*sizeof(struct fun_args), GFP_KERNEL);
	soft_kthread_init(linux_task, (long)linux_fun, 0, 0);
	rt_task_resume(linux_task);
	rt_request_linux_irq(RTAI_TIMER_8254_IRQ, handler, "rtaih", handler);
	return 0;
}

void cleanup_module(void)
{
	rt_free_linux_irq(RTAI_TIMER_8254_IRQ, handler);
	soft_kthread_delete(linux_task);
	rt_task_delete(&rtai_task);
	rt_sem_delete(&rtai_sem);
	rt_sem_delete(&linux_sem);
	rt_mbx_delete(&mbx);
}

