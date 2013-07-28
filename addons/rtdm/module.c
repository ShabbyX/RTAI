/**
 * @file
 * Real-Time Driver Model for RTAI
 *
 * @note Copyright (C) 2005 Jan Kiszka <jan.kiszka@web.de>
 * @note Copyright (C) 2005 Joerg Langenberg <joerg.langenberg@gmx.net>
 * @note Copyright (C) 2005-2010 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *       only for the adaption to RTAI.
 *
 * RTAI is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * RTAI is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RTAI; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*!
 * @defgroup rtdm Real-Time Driver Model
 *
 * The Real-Time Driver Model (RTDM) provides a unified interface to
 * both users and developers of real-time device
 * drivers. Specifically, it addresses the constraints of mixed
 * RT/non-RT systems like RTAI. RTDM conforms to POSIX
 * semantics (IEEE Std 1003.1) where available and applicable.
 */

/*!
 * @ingroup rtdm
 * @defgroup profiles Device Profiles
 *
 * Device profiles define which operation handlers a driver of a certain class
 * has to implement, which name or protocol it has to register, which IOCTLs
 * it has to provide, and further details. Sub-classes can be defined in order
 * to extend a device profile with more hardware-specific functions.
 */

#include <linux/module.h>
#include <linux/types.h>

#include <rtdm/rtdm.h>
#include <rtdm/internal.h>
#include <rtdm/rtdm_driver.h>

MODULE_DESCRIPTION("Real-Time Driver Model");
MODULE_AUTHOR("jan.kiszka@web.de");
MODULE_LICENSE("GPL");

static RTAI_SYSCALL_MODE int sys_rtdm_fdcount(void)
{
	return RTDM_FD_MAX;
}

static RTAI_SYSCALL_MODE int sys_rtdm_open(const char *path, long oflag)
{
	struct task_struct *curr = current;
	char krnl_path[RTDM_MAX_DEVNAME_LEN + 1];

	if (unlikely(!__xn_access_ok(curr, VERIFY_READ, path, sizeof(krnl_path)))) {
		return -EFAULT;
	}
	__xn_copy_from_user(curr, krnl_path, path, sizeof(krnl_path) - 1);
	krnl_path[sizeof(krnl_path) - 1] = '\0';
	return __rt_dev_open(curr, (const char *)krnl_path, oflag);
}

static RTAI_SYSCALL_MODE int sys_rtdm_socket(long protocol_family, long socket_type, long protocol)
{
	return __rt_dev_socket(current, protocol_family, socket_type, protocol);
}

static RTAI_SYSCALL_MODE int sys_rtdm_close(long fd, long forced)
{
	return __rt_dev_close(current, fd);
}

static RTAI_SYSCALL_MODE int sys_rtdm_ioctl(long fd, long request, void *arg)
{
	return __rt_dev_ioctl(current, fd, request, arg);
}

static RTAI_SYSCALL_MODE int sys_rtdm_read(long fd, void *buf, long nbytes)
{
	return __rt_dev_read(current, fd, buf, nbytes);
}

static RTAI_SYSCALL_MODE int sys_rtdm_write(long fd, void *buf, long nbytes)
{
	return __rt_dev_write(current, fd, buf, nbytes);
}

static RTAI_SYSCALL_MODE int sys_rtdm_recvmsg(long fd, struct msghdr *msg, long flags)
{
	struct msghdr krnl_msg;
	struct task_struct *curr = current;
	int ret;

	if (unlikely(!__xn_access_ok(curr, VERIFY_WRITE, msg, sizeof(krnl_msg)))) {
		return -EFAULT;
	}
	__xn_copy_from_user(curr, &krnl_msg, msg, sizeof(krnl_msg));
	if ((ret = __rt_dev_recvmsg(curr, fd, &krnl_msg, flags)) >= 0) {
		__xn_copy_to_user(curr, msg, &krnl_msg, sizeof(krnl_msg));
	}
	return ret;
}

static RTAI_SYSCALL_MODE int sys_rtdm_sendmsg(long fd, const struct msghdr *msg, long flags)
{
	struct msghdr krnl_msg;
	struct task_struct *curr = current;

	if (unlikely(!__xn_access_ok(curr, VERIFY_READ, msg, sizeof(krnl_msg)))) {
		return -EFAULT;
	}
	__xn_copy_from_user(curr, &krnl_msg, msg, sizeof(krnl_msg));
	return __rt_dev_sendmsg(curr, fd, &krnl_msg, flags);
}

static RTAI_SYSCALL_MODE int sys_rtdm_select(int nfds, fd_set *rfds, fd_set *wfds, fd_set *efds, nanosecs_rel_t timeout)
{
	return rt_dev_select(nfds, rfds, wfds, efds, timeout);
}

static struct rt_fun_entry rtdm[] = {
	[__rtdm_fdcount] = { 0, sys_rtdm_fdcount },
	[__rtdm_open]    = { 0, sys_rtdm_open },
	[__rtdm_socket]  = { 0, sys_rtdm_socket },
	[__rtdm_close]   = { 0, sys_rtdm_close },
	[__rtdm_ioctl]   = { 0, sys_rtdm_ioctl },
	[__rtdm_read]    = { 0, sys_rtdm_read },
	[__rtdm_write]   = { 0, sys_rtdm_write },
	[__rtdm_recvmsg] = { 0, sys_rtdm_recvmsg },
	[__rtdm_sendmsg] = { 0, sys_rtdm_sendmsg },
	[__rtdm_select]  = { 0, sys_rtdm_select },
};

/* This is needed because RTDM interrupt handlers:
 * - do no want immediate in handler rescheduling, RTAI can be configured
 *   to act in the same way but might not have been enabled to do so;
 * - may not reenable the PIC directly, assuming it will be done here;
 * - may not propagate, assuming it will be done here as well.
 * - might use shared interrupts its own way;
 * REMARK: RTDM irqs management is as generic as its pet system dictates
 *         and there is no choice but doing the same as closely as possible;
 *         so this is an as verbatim as possible copy of what is needed from
 *         the RTDM pet system.
 * REMINDER: the RTAI dispatcher cares mask/ack-ing anyhow, but RTDM will
 *           (must) provide the most suitable one for the shared case. */

#ifndef CONFIG_RTAI_SCHED_ISR_LOCK
extern struct { volatile int locked, rqsted; } rt_scheduling[];
extern void rtai_isr_sched_handle(int);

#define RTAI_SCHED_ISR_LOCK() \
	do { \
		int cpuid = rtai_cpuid(); \
		if (!rt_scheduling[cpuid].locked++) { \
			rt_scheduling[cpuid].rqsted = 0; \
		}
#define RTAI_SCHED_ISR_UNLOCK() \
		rtai_cli(); \
		if (rt_scheduling[cpuid].locked && !(--rt_scheduling[cpuid].locked)) { \
			if (rt_scheduling[cpuid].rqsted > 0) { \
				rtai_isr_sched_handle(cpuid); \
			} \
		} \
	} while (0)
#else /* !CONFIG_RTAI_SCHED_ISR_LOCK */
#define RTAI_SCHED_ISR_LOCK() \
	do {             } while (0)
#define RTAI_SCHED_ISR_UNLOCK() \
	do { rtai_cli(); } while (0)
#endif /* CONFIG_RTAI_SCHED_ISR_LOCK */

#define XNINTR_MAX_UNHANDLED	1000

DEFINE_PRIVATE_XNLOCK(intrlock);

static void xnintr_irq_handler(unsigned irq, void *cookie);

#ifdef CONFIG_RTAI_RTDM_SHIRQ

typedef struct xnintr_irq {

	DECLARE_XNLOCK(lock);

	xnintr_t *handlers;
	int unhandled;

} ____cacheline_aligned_in_smp xnintr_irq_t;

static xnintr_irq_t xnirqs[RTHAL_NR_IRQS];

static inline xnintr_t *xnintr_shirq_first(unsigned irq)
{
	return xnirqs[irq].handlers;
}

static inline xnintr_t *xnintr_shirq_next(xnintr_t *prev)
{
	return prev->next;
}

/*
 * Low-level interrupt handler dispatching the user-defined ISRs for
 * shared interrupts -- Called with interrupts off.
 */
static void xnintr_shirq_handler(unsigned irq, void *cookie)
{



	xnintr_irq_t *shirq = &xnirqs[irq];
	xnintr_t *intr;
	int s = 0;





	RTAI_SCHED_ISR_LOCK();

	xnlock_get(&shirq->lock);
	intr = shirq->handlers;

	while (intr) {
		int ret;

		ret = intr->isr(intr);
		s |= ret;

		if (ret & XN_ISR_HANDLED) {
			xnstat_counter_inc(
				&intr->stat[xnsched_cpu(sched)].hits);




		}

		intr = intr->next;
	}

	xnlock_put(&shirq->lock);

	if (unlikely(s == XN_ISR_NONE)) {
		if (++shirq->unhandled == XNINTR_MAX_UNHANDLED) {
			xnlogerr("%s: IRQ%d not handled. Disabling IRQ "
				 "line.\n", __FUNCTION__, irq);
			s |= XN_ISR_NOENABLE;
		}
	} else
		shirq->unhandled = 0;

	if (s & XN_ISR_PROPAGATE)
		xnarch_chain_irq(irq);
	else if (!(s & XN_ISR_NOENABLE))
		xnarch_end_irq(irq);

	RTAI_SCHED_ISR_UNLOCK();




}

/*
 * Low-level interrupt handler dispatching the user-defined ISRs for
 * shared edge-triggered interrupts -- Called with interrupts off.
 */
static void xnintr_edge_shirq_handler(unsigned irq, void *cookie)
{
	const int MAX_EDGEIRQ_COUNTER = 128;




	xnintr_irq_t *shirq = &xnirqs[irq];
	xnintr_t *intr, *end = NULL;
	int s = 0, counter = 0;





	RTAI_SCHED_ISR_LOCK();

	xnlock_get(&shirq->lock);
	intr = shirq->handlers;

	while (intr != end) {
		int ret, code;




		ret = intr->isr(intr);
		code = ret & ~XN_ISR_BITMASK;
		s |= ret;

		if (code == XN_ISR_HANDLED) {
			end = NULL;
			xnstat_counter_inc(
				&intr->stat[xnsched_cpu(sched)].hits);




		} else if (end == NULL)
			end = intr;

		if (counter++ > MAX_EDGEIRQ_COUNTER)
			break;

		if (!(intr = intr->next))
			intr = shirq->handlers;
	}

	xnlock_put(&shirq->lock);

	if (counter > MAX_EDGEIRQ_COUNTER)
		xnlogerr
		    ("xnintr_edge_shirq_handler() : failed to get the IRQ%d line free.\n",
		     irq);

	if (unlikely(s == XN_ISR_NONE)) {
		if (++shirq->unhandled == XNINTR_MAX_UNHANDLED) {
			xnlogerr("%s: IRQ%d not handled. Disabling IRQ "
				 "line.\n", __FUNCTION__, irq);
			s |= XN_ISR_NOENABLE;
		}
	} else
		shirq->unhandled = 0;

	if (s & XN_ISR_PROPAGATE)
		xnarch_chain_irq(irq);
	else if (!(s & XN_ISR_NOENABLE))
		xnarch_end_irq(irq);

	RTAI_SCHED_ISR_UNLOCK();




}

static inline int xnintr_irq_attach(xnintr_t *intr)
{
	xnintr_irq_t *shirq = &xnirqs[intr->irq];
	xnintr_t *prev, **p = &shirq->handlers;
	int err;

	if (intr->irq >= RTHAL_NR_IRQS)
		return -EINVAL;

	if (__testbits(intr->flags, XN_ISR_ATTACHED))
		return -EPERM;

	if ((prev = *p) != NULL) {
		/* Check on whether the shared mode is allowed. */
		if (!(prev->flags & intr->flags & XN_ISR_SHARED) ||
		    (prev->iack != intr->iack)
		    || ((prev->flags & XN_ISR_EDGE) !=
			(intr->flags & XN_ISR_EDGE)))
			return -EBUSY;

		/* Get a position at the end of the list to insert the new element. */
		while (prev) {
			p = &prev->next;
			prev = *p;
		}
	} else {
		/* Initialize the corresponding interrupt channel */
		void (*handler) (unsigned, void *) = &xnintr_irq_handler;

		if (intr->flags & XN_ISR_SHARED) {
			if (intr->flags & XN_ISR_EDGE)
				handler = &xnintr_edge_shirq_handler;
			else
				handler = &xnintr_shirq_handler;

		}
		shirq->unhandled = 0;

		err = xnarch_hook_irq(intr->irq, handler, intr->iack, intr);
		if (err)
			return err;
	}

	__setbits(intr->flags, XN_ISR_ATTACHED);

	intr->next = NULL;

	/* Add the given interrupt object. No need to synchronise with the IRQ
	   handler, we are only extending the chain. */
	*p = intr;

	return 0;
}

static inline int xnintr_irq_detach(xnintr_t *intr)
{
	xnintr_irq_t *shirq = &xnirqs[intr->irq];
	xnintr_t *e, **p = &shirq->handlers;
	int err = 0;

	if (intr->irq >= RTHAL_NR_IRQS)
		return -EINVAL;

	if (!__testbits(intr->flags, XN_ISR_ATTACHED))
		return -EPERM;

	__clrbits(intr->flags, XN_ISR_ATTACHED);

	while ((e = *p) != NULL) {
		if (e == intr) {
			/* Remove the given interrupt object from the list. */
			xnlock_get(&shirq->lock);
			*p = e->next;
			xnlock_put(&shirq->lock);



			/* Release the IRQ line if this was the last user */
			if (shirq->handlers == NULL)
				err = xnarch_release_irq(intr->irq);

			return err;
		}
		p = &e->next;
	}

	xnlogerr("attempted to detach a non previously attached interrupt "
		 "object.\n");
	return err;
}

#else /* !CONFIG_RTAI_RTDM_SHIRQ */

#ifdef CONFIG_SMP
typedef struct xnintr_irq {

	DECLARE_XNLOCK(lock);

} ____cacheline_aligned_in_smp xnintr_irq_t;

static xnintr_irq_t xnirqs[RTHAL_NR_IRQS];
#endif /* CONFIG_SMP */

static inline xnintr_t *xnintr_shirq_first(unsigned irq)
{
	return xnarch_get_irq_cookie(irq);
}

static inline xnintr_t *xnintr_shirq_next(xnintr_t *prev)
{
	return NULL;
}

static inline int xnintr_irq_attach(xnintr_t *intr)
{
	return xnarch_hook_irq(intr->irq, &xnintr_irq_handler, intr->iack, intr);
}

static inline int xnintr_irq_detach(xnintr_t *intr)
{
	int irq = intr->irq, err;

	xnlock_get(&xnirqs[irq].lock);
	err = xnarch_release_irq(irq);
	xnlock_put(&xnirqs[irq].lock);



	return err;
}

#endif /* !CONFIG_RTAI_RTDM_SHIRQ */

/*
 * Low-level interrupt handler dispatching non-shared ISRs -- Called with
 * interrupts off.
 */
static void xnintr_irq_handler(unsigned irq, void *cookie)
{

	xnintr_t *intr;


	int s;





	RTAI_SCHED_ISR_LOCK();

	xnlock_get(&xnirqs[irq].lock);

#ifdef CONFIG_SMP
	/* In SMP case, we have to reload the cookie under the per-IRQ lock
	   to avoid racing with xnintr_detach. */
	intr = xnarch_get_irq_cookie(irq);
	if (unlikely(!intr)) {
		s = 0;
		goto unlock_and_exit;
	}
#else
	/* cookie always valid, attach/detach happens with IRQs disabled */
	intr = cookie;
#endif
	s = intr->isr(intr);

	if (unlikely(s == XN_ISR_NONE)) {
		if (++intr->unhandled == XNINTR_MAX_UNHANDLED) {
			xnlogerr("%s: IRQ%d not handled. Disabling IRQ "
				 "line.\n", __FUNCTION__, irq);
			s |= XN_ISR_NOENABLE;
		}
	} else {
		xnstat_counter_inc(&intr->stat[xnsched_cpu(sched)].hits);



		intr->unhandled = 0;
	}

#ifdef CONFIG_SMP
 unlock_and_exit:
#endif
	xnlock_put(&xnirqs[irq].lock);

	if (s & XN_ISR_PROPAGATE)
		xnarch_chain_irq(irq);
	else if (!(s & XN_ISR_NOENABLE))
		xnarch_end_irq(irq);

	RTAI_SCHED_ISR_UNLOCK();




}

int xnintr_mount(void)
{
	int i;
	for (i = 0; i < RTHAL_NR_IRQS; ++i)
		xnlock_init(&xnirqs[i].lock);
	return 0;
}

int xnintr_init(xnintr_t *intr,
		const char *name,
		unsigned irq, xnisr_t isr, xniack_t iack, xnflags_t flags)
{
	intr->irq = irq;
	intr->isr = isr;
	intr->iack = iack;
	intr->cookie = NULL;
	intr->name = name ? : "<unknown>";
	intr->flags = flags;
	intr->unhandled = 0;
	memset(&intr->stat, 0, sizeof(intr->stat));
#ifdef CONFIG_RTAI_RTDM_SHIRQ
	intr->next = NULL;
#endif

	return 0;
}

int xnintr_destroy(xnintr_t *intr)
{
	xnintr_detach(intr);
	return 0;
}

int xnintr_attach(xnintr_t *intr, void *cookie)
{
	int err;
	spl_t s;




	intr->cookie = cookie;
	memset(&intr->stat, 0, sizeof(intr->stat));

	xnlock_get_irqsave(&intrlock, s);

#ifdef CONFIG_SMP
	xnarch_set_irq_affinity(intr->irq, nkaffinity);
#endif /* CONFIG_SMP */
	err = xnintr_irq_attach(intr);




	xnlock_put_irqrestore(&intrlock, s);

	return err;
}

int xnintr_detach(xnintr_t *intr)
{
	int err;
	spl_t s;



	xnlock_get_irqsave(&intrlock, s);

	err = xnintr_irq_detach(intr);




	xnlock_put_irqrestore(&intrlock, s);

	return err;
}

int xnintr_enable (xnintr_t *intr)
{

	rt_enable_irq(intr->irq);
	return 0;
}

int xnintr_disable (xnintr_t *intr)
{

	rt_disable_irq(intr->irq);
	return 0;
}

extern struct epoch_struct boot_epoch;

#ifdef CONFIG_SMP
#define NUM_CPUS           RTAI_NR_CPUS
#define TIMED_TIMER_CPUID  (timed_timer->cpuid)
#define TIMER_CPUID        (timer->cpuid)
#define LIST_CPUID         (cpuid)
#else
#define NUM_CPUS           1
#define TIMED_TIMER_CPUID  (0)
#define TIMER_CPUID        (0)
#define LIST_CPUID         (0)
#endif

static struct rtdm_timer_struct timers_list[NUM_CPUS] =
{ { &timers_list[0], &timers_list[0], RT_SCHED_LOWEST_PRIORITY, 0, RT_TIME_END, 0LL, NULL, 0UL,
#ifdef  CONFIG_RTAI_LONG_TIMED_LIST
{ NULL }
#endif
}, };

//static spinlock_t timers_lock[NUM_CPUS] = { SPIN_LOCK_UNLOCKED, };
static spinlock_t timers_lock[NUM_CPUS] = { __SPIN_LOCK_UNLOCKED(timers_lock[0]), };


#ifdef CONFIG_RTAI_LONG_TIMED_LIST

/* BINARY TREE */
static inline void enq_timer(struct rtdm_timer_struct *timed_timer)
{
	struct rtdm_timer_struct *timerh, *tmrnxt, *timer;
	rb_node_t **rbtn, *rbtpn = NULL;
	timer = timerh = &timers_list[TIMED_TIMER_CPUID];
	rbtn = &timerh->rbr.rb_node;

	while (*rbtn) {
		rbtpn = *rbtn;
		tmrnxt = rb_entry(rbtpn, struct rtdm_timer_struct, rbn);
		if (timer->firing_time > tmrnxt->firing_time) {
			rbtn = &(rbtpn)->rb_right;
		} else {
			rbtn = &(rbtpn)->rb_left;
			timer = tmrnxt;
		}
	}
	rb_link_node(&timed_timer->rbn, rbtpn, rbtn);
	rb_insert_color(&timed_timer->rbn, &timerh->rbr);
	timer->prev = (timed_timer->prev = timer->prev)->next = timed_timer;
	timed_timer->next = timer;
}

#define rb_erase_timer(timer) \
rb_erase(&(timer)->rbn, &timers_list[NUM_CPUS > 1 ? (timer)->cpuid : 0].rbr)

#else /* !CONFIG_RTAI_LONG_TIMED_LIST */

/* LINEAR */
static inline void enq_timer(struct rtdm_timer_struct *timed_timer)
{
	struct rtdm_timer_struct *timer;
	timer = &timers_list[TIMED_TIMER_CPUID];
	while (timed_timer->firing_time > (timer = timer->next)->firing_time);
	timer->prev = (timed_timer->prev = timer->prev)->next = timed_timer;
	timed_timer->next = timer;
}

#define rb_erase_timer(timer)

#endif /* CONFIG_RTAI_LONG_TIMED_LIST */

static inline void rem_timer(struct rtdm_timer_struct *timer)
{
	(timer->next)->prev = timer->prev;
	(timer->prev)->next = timer->next;
	timer->next = timer->prev = timer;
	rb_erase_timer(timer);
}

static RT_TASK timers_manager[NUM_CPUS];

static inline void asgn_min_prio(int cpuid)
{
	RT_TASK *timer_manager;
	struct rtdm_timer_struct *timer, *timerl;
	spinlock_t *lock;
	unsigned long flags;
	int priority;

	priority = (timer = (timerl = &timers_list[LIST_CPUID])->next)->priority;
	flags = rt_spin_lock_irqsave(lock = &timers_lock[LIST_CPUID]);
	while ((timer = timer->next) != timerl) {
		if (timer->priority < priority) {
			priority = timer->priority;
		}
		rt_spin_unlock_irqrestore(flags, lock);
		flags = rt_spin_lock_irqsave(lock);
	}
	rt_spin_unlock_irqrestore(flags, lock);
	flags = rt_global_save_flags_and_cli();
	if ((timer_manager = &timers_manager[LIST_CPUID])->priority > priority) {
		timer_manager->priority = priority;
		if (timer_manager->state == RT_SCHED_READY) {
			rem_ready_task(timer_manager);
			enq_ready_task(timer_manager);
		}
	}
	rt_global_restore_flags(flags);
}

RTAI_SYSCALL_MODE int rt_timer_insert(struct rtdm_timer_struct *timer, int priority, RTIME firing_time, RTIME period, void (*handler)(unsigned long), unsigned long data)
{
	spinlock_t *lock;
	unsigned long flags, cpuid;
	RT_TASK *timer_manager;

	if (!handler) {
		return -EINVAL;
	}
	timer->handler     = handler;
	timer->data        = data;
	timer->priority    = priority;
	timer->firing_time = firing_time;
	timer->period      = period;
	REALTIME2COUNT(firing_time)

	timer->cpuid = cpuid = NUM_CPUS > 1 ? rtai_cpuid() : 0;
// timer insertion in timers_list
	flags = rt_spin_lock_irqsave(lock = &timers_lock[LIST_CPUID]);
	enq_timer(timer);
	rt_spin_unlock_irqrestore(flags, lock);
// timers_manager priority inheritance
	if (timer->priority < (timer_manager = &timers_manager[LIST_CPUID])->priority) {
		timer_manager->priority = timer->priority;
	}
// timers_task deadline inheritance
	flags = rt_global_save_flags_and_cli();
	if (timers_list[LIST_CPUID].next == timer && (timer_manager->state & RT_SCHED_DELAYED) && firing_time < timer_manager->resume_time) {
		timer_manager->resume_time = firing_time;
		rem_timed_task(timer_manager);
		enq_timed_task(timer_manager);
		rt_schedule();
	}
	rt_global_restore_flags(flags);
	return 0;
}

RTAI_SYSCALL_MODE void rt_timer_remove(struct rtdm_timer_struct *timer)
{
	if (timer->next != timer && timer->prev != timer) {
		spinlock_t *lock;
		unsigned long flags;
		flags = rt_spin_lock_irqsave(lock = &timers_lock[TIMER_CPUID]);
		rem_timer(timer);
		rt_spin_unlock_irqrestore(flags, lock);
		asgn_min_prio(TIMER_CPUID);
	}
}

static int TimersManagerPrio = 0;
RTAI_MODULE_PARM(TimersManagerPrio, int);

static void rt_timers_manager(long cpuid)
{
	RTIME now;
	RT_TASK *timer_manager;
	struct rtdm_timer_struct *tmr, *timer, *timerl;
	spinlock_t *lock;
	unsigned long flags, timer_tol;
	int priority;

	timer_manager = &timers_manager[LIST_CPUID];
	timerl = &timers_list[LIST_CPUID];
	lock = &timers_lock[LIST_CPUID];
	timer_tol = tuned.timers_tol[LIST_CPUID];

	while (1) {
		rt_sleep_until((timerl->next)->firing_time);
		now = rt_get_time() + timer_tol;
		while (1) {
			tmr = timer = timerl;
			priority = RT_SCHED_LOWEST_PRIORITY;
			flags = rt_spin_lock_irqsave(lock);
			while ((tmr = tmr->next)->firing_time <= now) {
				if (tmr->priority < priority) {
					priority = (timer = tmr)->priority;
				}
			}
			rt_spin_unlock_irqrestore(flags, lock);
			if (timer == timerl) {
				if (timer_manager->priority > TimersManagerPrio) {
					timer_manager->priority = TimersManagerPrio;
				}
				break;
			}
			timer_manager->priority = priority;
			flags = rt_spin_lock_irqsave(lock);
			rem_timer(timer);
			if (timer->period) {
				timer->firing_time += timer->period;
				enq_timer(timer);
			}
			rt_spin_unlock_irqrestore(flags, lock);
			timer->handler(timer->data);
		}
		asgn_min_prio(LIST_CPUID);
	}
}

static int TimersManagerStacksize = 8192;
RTAI_MODULE_PARM(TimersManagerStacksize, int);

static int rtai_timers_init(void)
{
	int cpuid;

	for (cpuid = 0; cpuid < num_online_cpus(); cpuid++) {
		timers_lock[cpuid] = timers_lock[0];
		timers_list[cpuid] = timers_list[0];
		timers_list[cpuid].cpuid = cpuid;
		timers_list[cpuid].next = timers_list[cpuid].prev = &timers_list[cpuid];
	}
	for (cpuid = 0; cpuid < num_online_cpus(); cpuid++) {
		rt_task_init_cpuid(&timers_manager[cpuid], rt_timers_manager, cpuid, TimersManagerStacksize, TimersManagerPrio, 0, NULL, cpuid);
		rt_task_resume(&timers_manager[cpuid]);
	}
	return 0;
}

static void rtai_timers_cleanup(void)
{
	int cpuid;
	for (cpuid = 0; cpuid < num_online_cpus(); cpuid++) {
		rt_task_delete(&timers_manager[cpuid]);
	}
}

EXPORT_SYMBOL(rt_timer_insert);
EXPORT_SYMBOL(rt_timer_remove);

int __init rtdm_skin_init(void)
{
	int err;

	rtai_timers_init();
	if(set_rt_fun_ext_index(rtdm, RTDM_INDX)) {
		printk("LXRT extension %d already in use. Recompile RTDM with a different extension index\n", RTDM_INDX);
		return -EACCES;
	}
	if ((err = rtdm_dev_init())) {
		goto fail;
	}

	xnintr_mount();

#ifdef CONFIG_RTAI_RTDM_SELECT
	if (xnselect_mount()) {
		goto cleanup_core;
	}
#endif
#ifdef CONFIG_PROC_FS
	if ((err = rtdm_proc_init())) {
		goto cleanup_core;
	}
#endif /* CONFIG_PROC_FS */

	printk("RTDM started.\n");
	return 0;

cleanup_core:
#ifdef CONFIG_RTAI_RTDM_SELECT
	xnselect_umount();
#endif
	rtdm_dev_cleanup();
#ifdef CONFIG_PROC_FS
	rtdm_proc_cleanup();
#endif /* CONFIG_PROC_FS */
fail:
	return err;
}

void __exit rtdm_skin_exit(void)
{
#ifdef CONFIG_RTAI_RTDM_SELECT
	xnselect_umount();
#endif
	rtai_timers_cleanup();
	rtdm_dev_cleanup();
	reset_rt_fun_ext_index(rtdm, RTDM_INDX);
#ifdef CONFIG_PROC_FS
	rtdm_proc_cleanup();
#endif /* CONFIG_PROC_FS */
	printk("RTDM stopped.\n");
}

module_init(rtdm_skin_init);
module_exit(rtdm_skin_exit);

EXPORT_SYMBOL(xnintr_init);
EXPORT_SYMBOL(xnintr_destroy);
EXPORT_SYMBOL(xnintr_attach);
EXPORT_SYMBOL(xnintr_detach);
EXPORT_SYMBOL(xnintr_disable);
EXPORT_SYMBOL(xnintr_enable);
