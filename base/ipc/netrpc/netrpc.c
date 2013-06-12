/*
 * Copyright (C) 1999-2009 Paolo Mantegazza <mantegazza@aero.polimi.it>
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


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/timer.h>
#include <linux/unistd.h>
#include <asm/uaccess.h>

#include <net/ip.h>

#include <rtai_schedcore.h>
#include <rtai_prinher.h>
#include <rtai_netrpc.h>
#include <rtai_sem.h>
#include <rtai_mbx.h>

MODULE_LICENSE("GPL");

#define COMPILE_ANYHOW  // RTNet is not available but we want to compile anyhow
#include <rtdm.h>
#include "rtnetP.h"

/* ethernet support(s) we want to use: 1 -> DO, 0 -> DO NOT */

#define SOFT_RTNET  1

#ifdef CONFIG_RTAI_NETRPC_RTNET
#define HARD_RTNET  1
#else
#define HARD_RTNET  0
#endif

/* end of ethernet support(s) we want to use */

#if HARD_RTNET
#ifndef COMPILE_ANYHOW
#include <rtnet.h>  // must be the true RTNet header file
#endif
#define MSG_SOFT  0
#define MSG_HARD  1
#define hard_rt_socket           	rt_dev_socket
#define hard_rt_bind             	rt_dev_bind
#define hard_rt_close            	rt_dev_close
#define hard_rt_socket_callback		hard_rt_socket_callback
#define hard_rt_recvfrom         	rt_dev_recvfrom
#define hard_rt_sendto           	rt_dev_sendto
#else
#define MSG_SOFT 0
#define MSG_HARD 1
#define hard_rt_socket(a, b, c)  	portslot[i].socket[0]
#define hard_rt_bind(a, b, c)
#define hard_rt_close(a)
#define hard_rt_socket_callback  	soft_rt_socket_callback
#define hard_rt_recvfrom         	soft_rt_recvfrom
#define hard_rt_sendto           	soft_rt_sendto
#endif

#define LOCALHOST  "127.0.0.1"

#define BASEPORT  (NETRPC_BASEPORT)

#define NETRPC_STACK_SIZE  6000

static unsigned long MaxStubs = MAX_STUBS;
RTAI_MODULE_PARM(MaxStubs, ulong);
static int MaxStubsMone;

static unsigned long MaxSocks = MAX_SOCKS;
RTAI_MODULE_PARM(MaxSocks, ulong);

static int StackSize = NETRPC_STACK_SIZE;
RTAI_MODULE_PARM(StackSize, int);

static char *ThisNode = LOCALHOST;
RTAI_MODULE_PARM(ThisNode, charp);

static char *ThisSoftNode = 0;
RTAI_MODULE_PARM(ThisSoftNode, charp);

static char *ThisHardNode = 0;
RTAI_MODULE_PARM(ThisHardNode, charp);

#define MAX_DFUN_EXT  16
//static struct rt_fun_entry *rt_net_rpc_fun_ext[MAX_DFUN_EXT];
extern struct rt_fun_entry *rt_fun_ext[];
#define rt_net_rpc_fun_ext rt_fun_ext

static unsigned long this_node[2];

#define PRTSRVNAME  0xFFFFFFFF
struct portslot_t { struct portslot_t *p; long task; int indx, place, socket[2], hard; unsigned long long owner; SEM sem; void *msg; struct sockaddr_in addr; MBX *mbx; unsigned long name;  RTIME timeout; int recovered; };
static DEFINE_SPINLOCK(portslot_lock);
static volatile int portslotsp;

static DEFINE_SPINLOCK(stub_lock);
static volatile int stubssp = 1;


static struct portslot_t *portslot;
static struct sockaddr_in SPRT_ADDR;
struct recovery_msg { int hard, priority; unsigned long long owner; struct sockaddr addr; };
static struct { int in, out; struct recovery_msg *msg; } recovery;
static DEFINE_SPINLOCK(recovery_lock);

#if HARD_RTNET
int hard_rt_socket_callback(int fd, void *func, void *arg)
{
    struct rtnet_callback args = { func, arg };
    return(rt_dev_ioctl(fd, RTNET_RTIOC_CALLBACK, &args));
}
#endif

static inline struct portslot_t *get_portslot(void)
{
	unsigned long flags;

	flags = rt_spin_lock_irqsave(&portslot_lock);
	if (portslotsp < MaxSocks) {
		struct portslot_t *p;
		p = portslot[portslotsp++].p;
		rt_spin_unlock_irqrestore(flags, &portslot_lock);
		return p;
	}
	rt_spin_unlock_irqrestore(flags, &portslot_lock);
	return 0;
}

static inline int gvb_portslot(struct portslot_t *portslotp)
{
	unsigned long flags;

	flags = rt_spin_lock_irqsave(&portslot_lock);
	if (portslotsp > MaxStubs) {
		struct portslot_t *tmp_p;
		int tmp_place;
		tmp_p = portslot[--portslotsp].p;
		tmp_place = portslotp->place;
		portslotp->place = portslotsp;
		portslot[portslotsp].p = portslotp;
		tmp_p->place = tmp_place;
		portslot[tmp_place].p = tmp_p;
		rt_spin_unlock_irqrestore(flags, &portslot_lock);
		return 0;
	}
	rt_spin_unlock_irqrestore(flags, &portslot_lock);
	return -EINVAL;
}

static inline void check_portslot(unsigned long node, int port, struct portslot_t **p)
{
//	unsigned long flags;
	int i;
	struct portslot_t *p_old;

	p_old = *p;
//	flags = rt_spin_lock_irqsave(&portslot_lock);
	for (i = MaxStubs; i < portslotsp; i++) {
		if (portslot[i].p->addr.sin_port == htons(port) && portslot[i].p->addr.sin_addr.s_addr == node) {
			*p = portslot[i].p;
			break;
		}
	}
//	rt_spin_unlock_irqrestore(flags, &portslot_lock);
	if (p_old != *p)	{
		gvb_portslot(p_old);
	}

}

#define NETRPC_TIMER_FREQ 50
static struct timer_list timer;
static SEM timer_sem;

static void timer_fun(unsigned long none)
{
	if (timer_sem.count < 0) {
		rt_sem_signal(&timer_sem);
		timer.expires = jiffies + HZ/NETRPC_TIMER_FREQ;
		add_timer(&timer);
	}
}

static int (*encode)(struct portslot_t *portslotp, void *msg, int size, int where);
static int (*decode)(struct portslot_t *portslotp, void *msg, int size, int where);

static void *encdec_ext;
void set_netrpc_encoding(void *encode_fun, void *decode_fun, void *ext)
{
	encode = encode_fun;
	decode = decode_fun;
	if (!set_rt_fun_ext_index(ext, 1)) {
		encdec_ext = ext;
	}
//	rt_net_rpc_fun_ext[1] = ext;
}

struct req_rel_msg { long long op, port, priority, hard; unsigned long long owner, name, rem_node, chkspare;};

struct par_t { long long mach, priority, base_priority, argsize, rsize, fun_ext_timed, type; unsigned long long owner, partypes; long a[1]; };

struct reply_t { long long wsize, w2size, myport; unsigned long long retval; char msg[1], msg1[1]; };

static inline int argconv(void *ain, void *aout, int send_mach, int argsize, unsigned int partypes)
{
#define recv_mach  (sizeof(long))
	int argsizeout;
	if (send_mach == recv_mach) {
		memcpy(aout, ain, argsize);
		return argsize;
	}
	argsizeout = 0;
	if (send_mach == 4 && recv_mach == 8) {
		long *out = aout;
		int *in = ain;
		while (argsize) {
			argsize -= 4;
			argsizeout += 8;
			switch(partypes & WDWMSK) {
				case SINT:
					*out++ = *in++;
					break;
				case UINT:
					*out++ = (unsigned int)*in++;
					break;
				case VADR:
					*out++ = reset_kadr(*in++);
					break;
				case RTIM:
					*out++ = (long)*in++;
					in++;
					argsize -= 4;
					break;
			}
			partypes >>= WDW;
		}
	} else {
		unsigned long *out = aout;
		unsigned long long *in = ain;
		while (argsize) {
			argsize -= 8;
			argsizeout += 4;
			switch(partypes & WDWMSK) {
				case SINT:
				case UINT:
				case VADR:
					*out++ = *in++;
					break;
				case RTIM:
					*((unsigned long long *)out++) = *in++;
					out++;
					argsizeout += 4;
					break;
			}
			partypes >>= WDW;
		}
	}
	return argsizeout;
#undef recv_mach
}

static void net_resume_task(int sock, struct portslot_t *p)
{
	int all_ok;
	RT_TASK *my;
	my = _rt_whoami();
	all_ok = 1;
	if ((p->indx > 0) && (p->indx < MaxStubs)) {
		if (!((p->task) && (p->hard == my->is_hard))) {
			all_ok = 0;
		}
	}
	if (all_ok) {
		rt_sem_signal(&p->sem);
	} else {
		long i;
		unsigned long flags;
		struct par_t *par;
		char msg[MAX_MSG_SIZE];
		struct sockaddr *addr;
		par = (void *)msg;
		addr = (struct sockaddr*)&p->addr;

		if (my->is_hard) {
			while (hard_rt_recvfrom(p->socket[1], msg, MAX_MSG_SIZE, 0, addr, (void *)&i) == -EAGAIN);
		} else {
			soft_rt_recvfrom(p->socket[0], msg, MAX_MSG_SIZE, 0, addr, &i);
		}

		flags = rt_spin_lock_irqsave(&recovery_lock);
		recovery.msg[recovery.in].priority = par->priority;
		recovery.msg[recovery.in].owner = par->owner;
		recovery.msg[recovery.in].addr = *addr;
		recovery.msg[recovery.in].hard = my->is_hard;
		recovery.in = (recovery.in + 1) & MaxStubsMone;
		rt_spin_unlock_irqrestore(flags, &recovery_lock);
		rt_sem_signal(&portslot[0].sem);
	}
}

int get_min_tasks_cpuid(void);
int set_rtext(RT_TASK *, int, int, void(*)(void), unsigned int, void *);
int clr_rtext(RT_TASK *);
void rt_schedule_soft(RT_TASK *);

static inline int soft_rt_fun_call(RT_TASK *task, void *fun, void *arg)
{
	union { long long ll; long l; } retval;
	task->fun_args[0] = (long)arg;
	((struct fun_args *)task->fun_args)->fun = fun;
	rt_schedule_soft(task);
	retval.ll = task->retval;
	return (int)retval.l;
}

static inline long long soft_rt_genfun_call(RT_TASK *task, void *fun, void *args, int argsize)
{
	memcpy(task->fun_args, args, argsize);
	((struct fun_args *)task->fun_args)->fun = fun;
	rt_schedule_soft(task);
	return task->retval;
}

extern void rt_daemonize(void);
static void thread_fun(RT_TASK *task)
{
	if (!set_rtext(task, task->fun_args[3], 0, 0, get_min_tasks_cpuid(), 0)) {
		rt_daemonize();
		sigfillset(&current->blocked);
		rtai_set_linux_task_priority(current, SCHED_FIFO, MIN_LINUX_RTPRIO);
		soft_rt_fun_call(task, rt_task_suspend, task);
		((void (*)(long))task->fun_args[1])(task->fun_args[2]);
		task->fun_args[1] = 0;
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
			schedule_timeout(HZ/NETRPC_TIMER_FREQ);
		}
		return 0;
	}
	return -ENOEXEC;
}

static int soft_kthread_delete(RT_TASK *task)
{
	task->fun_args[1] = 1;
	while (task->fun_args[1]) {
		rt_task_masked_unblock(task, ~RT_SCHED_READY);
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(HZ/NETRPC_TIMER_FREQ);
	}
	clr_rtext(task);
	return 0;
}

#define ADRSZ  sizeof(struct sockaddr)

static inline int get_stub(unsigned long long owner)
{
	unsigned long flags;
	int i;

	flags = rt_spin_lock_irqsave(&stub_lock);
	if (stubssp < MaxStubsMone) {
		struct portslot_t *p;
		i = 1;
		while(((p = portslot[i].p)->owner != owner) && (i < stubssp)) {
			i++;
		}
		if (p->owner != owner) {
			p = portslot[stubssp++].p;
			p->owner = owner;
		}
		rt_spin_unlock_irqrestore(flags, &stub_lock);
		return p->indx;
	}
	rt_spin_unlock_irqrestore(flags, &stub_lock);
	return 0;
}

static inline int gvb_stub(int slot, unsigned long long owner)
{
	unsigned long flags;
	RT_TASK *task;

	flags = rt_spin_lock_irqsave(&stub_lock);
	if (stubssp > 1) {
		if (slot > 0 && slot < MaxStubs) {
			if (portslot[slot].owner == owner) {
				struct portslot_t *tmp_p;
				int tmp_place;
				task = (RT_TASK *)portslot[slot].task;
				portslot[slot].task = 0;
				portslot[slot].owner = 0;
				tmp_p = portslot[--stubssp].p;
				tmp_place = portslot[slot].place;
				portslot[slot].place = stubssp;
				portslot[stubssp].p = &portslot[slot];
				tmp_p->place = tmp_place;
				portslot[tmp_place].p = tmp_p;
				slot += BASEPORT;
				rt_spin_unlock_irqrestore(flags, &stub_lock);
				if (task->is_hard) {
					rt_task_delete(task);
				} else {
					soft_kthread_delete(task);
				}
				kfree(task);
			} else {
				slot = !portslot[slot].owner ? slot + BASEPORT : -ENXIO;
				rt_spin_unlock_irqrestore(flags, &stub_lock);
			}
		} else {
			rt_spin_unlock_irqrestore(flags, &stub_lock);
		}
		return slot;
	}
	rt_spin_unlock_irqrestore(flags, &stub_lock);
	return -EINVAL;
}

static inline int find_stub(unsigned long long owner)
{
	unsigned long flags;
	int i;
	struct portslot_t *p;
	i = 1;
	flags = rt_spin_lock_irqsave(&stub_lock);
	while(((p = portslot[i].p)->owner != owner) && (i < stubssp)) {
		i++;
	}
	rt_spin_unlock_irqrestore(flags, &stub_lock);
	return p->owner != owner ? 0 : p->indx;
}

static void soft_stub_fun(struct portslot_t *portslotp)
{
	char msg[MAX_MSG_SIZE];
	struct sockaddr *addr;
	RT_TASK *task;
	SEM *sem;
	struct par_t *par;
	long wsize, w2size, sock;
	long *ain;
	long type;

	addr = (struct sockaddr *)&portslotp->addr;
	sock = portslotp->socket[0];
	sem  = &portslotp->sem;
	ain = (par = (void *)msg)->a;
	task = (RT_TASK *)portslotp->task;
	sprintf(current->comm, "SFTSTB-%ld", sock);

recvrys:

	while (soft_rt_fun_call(task, rt_sem_wait, sem) < RTE_LOWERR) {
		wsize = soft_rt_recvfrom(sock, msg, MAX_MSG_SIZE, 0, addr, &w2size);
		if (decode) {
			decode(portslotp, msg, wsize, RPC_SRV);
		}
		if (portslotp->owner != par->owner)	{
			unsigned long flags;

			flags = rt_spin_lock_irqsave(&recovery_lock);
			recovery.msg[recovery.in].priority = par->priority;
			recovery.msg[recovery.in].owner = par->owner;
			recovery.msg[recovery.in].addr = *addr;
			recovery.msg[recovery.in].hard = 0;
			recovery.in = (recovery.in + 1) & MaxStubsMone;
			rt_spin_unlock_irqrestore(flags, &recovery_lock);
			rt_sem_signal(&portslot[0].sem);
		} else {
			int argsize;
			long a[par->argsize/sizeof(long) + 1];
			if(par->priority >= 0 && par->priority < RT_SCHED_LINUX_PRIORITY) {
				if ((wsize = par->priority) < task->priority) {
					task->priority = wsize;
					rtai_set_linux_task_priority(task->lnxtsk, task->lnxtsk->policy, wsize >= MAX_LINUX_RTPRIO ? MIN_LINUX_RTPRIO : MAX_LINUX_RTPRIO - wsize);
				}
				task->base_priority = par->base_priority;
			}
			argsize = argconv(ain, a, par->mach, par->argsize, par->partypes);
			type = par->type;
			if (par->rsize) {
				a[USP_RBF1(type) - 1] = (long)((char *)ain + par->argsize);
			}
			if (NEED_TO_W(type)) {
				wsize = USP_WSZ1(type);
				wsize = wsize ? a[wsize - 1] : par->mach; //sizeof(long);
			} else {
				wsize = 0;
			}
			if (NEED_TO_W2ND(type)) {
				w2size = USP_WSZ2(type);
				w2size = w2size ? a[w2size - 1] : par->mach; //sizeof(long);
			} else {
				w2size = 0;
			}
			do {
				struct msg_t { struct reply_t arg; char bufspace[wsize + w2size]; } arg;
				arg.arg.myport = 0;
				if (wsize > 0) {
					arg.arg.wsize = wsize;
					a[USP_WBF1(type) - 1] = (long)arg.arg.msg;
					if ((USP_WBF1(type) - 1) == (USP_RBF1(type) - 1) && wsize == par->rsize) {
						memcpy(arg.arg.msg, (char *)ain + par->argsize, wsize);
						a[USP_RBF1(type) - 1] = (long)(arg.arg.msg);
					}
				} else {
					arg.arg.wsize = 0;
				}
				if (w2size > 0) {
					arg.arg.w2size = w2size;
					a[USP_WBF2(type) - 1] = (long)(arg.arg.msg + arg.arg.wsize);
				} else {
					arg.arg.w2size = 0;
				}
#ifndef NETRPC_ALIGN_RTIME
				if ((wsize = TIMED(par->fun_ext_timed) - 1) >= 0) {
#else
				if ((wsize = TIMED(par->fun_ext_timed)) > 0) {
					wsize += (NETRPC_ALIGN_RTIME(wsize) - 1);
#endif
					*((long long *)(a + wsize)) = nano2count(*((long long *)(a + wsize)));
				}
				arg.arg.retval = soft_rt_genfun_call(task, rt_net_rpc_fun_ext[EXT(par->fun_ext_timed)][FUN(par->fun_ext_timed)].fun, a, argsize);
				soft_rt_sendto(sock, &arg, encode ? encode(portslotp, &arg, sizeof(struct msg_t), RPC_RTR) : sizeof(struct msg_t), 0, addr, ADRSZ);
			} while (0);
		}
	}
	if (portslotp->recovered) {
		struct reply_t arg;
		portslotp->recovered = 0;
		arg.myport = sock + BASEPORT;
		arg.retval = portslotp->owner;
		soft_rt_sendto(sock, &arg, encode ? encode(portslotp, &arg, sizeof(struct reply_t), RPC_RTR) : sizeof(struct reply_t), 0, addr, ADRSZ);
		goto recvrys;
	}
//	soft_rt_fun_call(task, rt_task_suspend, task);
}

static void hard_stub_fun(struct portslot_t *portslotp)
{
	char msg[MAX_MSG_SIZE];
	struct sockaddr *addr;
	RT_TASK *task;
	SEM *sem;
	struct par_t *par;
	long wsize, w2size, sock;
	long *ain;
	long type;

	addr = (struct sockaddr *)&portslotp->addr;
	sock = portslotp->socket[1];
	sem  = &portslotp->sem;
	ain = (par = (void *)msg)->a;
	task = (RT_TASK *)portslotp->task;
	if (task->lnxtsk) {
		sprintf(current->comm, "HRDSTB-%ld", sock);
	}

recvryh:

	while (rt_sem_wait(sem) < RTE_LOWERR) {
		wsize = hard_rt_recvfrom(sock, msg, MAX_MSG_SIZE, 0, addr, (void *)&w2size);
		if (decode) {
			decode(portslotp, msg, wsize, RPC_SRV);
		}
		if (portslotp->owner != par->owner)	{
			unsigned long flags;

			flags = rt_spin_lock_irqsave(&recovery_lock);
			recovery.msg[recovery.in].priority = par->priority;
			recovery.msg[recovery.in].owner = par->owner;
			recovery.msg[recovery.in].addr = *addr;
			recovery.msg[recovery.in].hard = 1;
			recovery.in = (recovery.in + 1) & MaxStubsMone;
			rt_spin_unlock_irqrestore(flags, &recovery_lock);
			rt_sem_signal(&portslot[0].sem);
		} else {
			int argsize;
			long a[par->argsize/sizeof(long) + 1];
			if(par->priority >= 0 && par->priority < RT_SCHED_LINUX_PRIORITY) {
				if ((wsize = par->priority) < task->priority) {
					task->priority = wsize;
				}
				task->base_priority = par->base_priority;
			}
			argsize = argconv(ain, a, par->mach, par->argsize, par->partypes);
			type = par->type;
			if (par->rsize) {
				a[USP_RBF1(type) - 1] = (long)((char *)ain + par->argsize);
			}
			if (NEED_TO_W(type)) {
				wsize = USP_WSZ1(type);
				wsize = wsize ? a[wsize - 1] : par->mach; //sizeof(long);
			} else {
				wsize = 0;
			}
			if (NEED_TO_W2ND(type)) {
				w2size = USP_WSZ2(type);
				w2size = w2size ? a[w2size - 1] : par->mach; //sizeof(long);
			} else {
				w2size = 0;
			}
			do {
				struct msg_t { struct reply_t arg; char bufspace[wsize + w2size]; } arg;
				arg.arg.myport = 0;
				if (wsize > 0) {
					arg.arg.wsize = wsize;
					if ((USP_WBF1(type) - 1) == (USP_RBF1(type) - 1) && wsize == par->rsize) {
						memcpy(arg.arg.msg, (char *)ain + par->argsize, wsize);
						a[USP_RBF1(type) - 1] = (long)(arg.arg.msg);
					}
					a[USP_WBF1(type) - 1] = (long)arg.arg.msg;
				} else {
					arg.arg.wsize = 0;
				}
				if (w2size > 0) {
					arg.arg.w2size = w2size;
					a[USP_WBF2(type) - 1] = (long)(arg.arg.msg + arg.arg.wsize);
				} else {
					arg.arg.w2size = 0;
				}
#ifndef NETRPC_ALIGN_RTIME
				if ((wsize = TIMED(par->fun_ext_timed) - 1) >= 0) {
#else
				if ((wsize = TIMED(par->fun_ext_timed)) > 0) {
					wsize += (NETRPC_ALIGN_RTIME(wsize) - 1);
#endif
					*((long long *)(a + wsize)) = nano2count(*((long long *)(a + wsize)));
				}
				arg.arg.retval = ((long long (*)(long, ...))rt_net_rpc_fun_ext[EXT(par->fun_ext_timed)][FUN(par->fun_ext_timed)].fun)(RTAI_FUN_A);
				hard_rt_sendto(sock, &arg, encode ? encode(portslotp, &arg, sizeof(struct msg_t), RPC_RTR) : sizeof(struct msg_t), 0, addr, ADRSZ);
			} while (0);
		}
	}
	if (portslotp->recovered) {
		struct reply_t arg;
		portslotp->recovered = 0;
		arg.myport = sock + BASEPORT;
		arg.retval = portslotp->owner;
		hard_rt_sendto(sock, &arg, encode ? encode(portslotp, &arg, sizeof(struct reply_t), RPC_RTR) : sizeof(struct reply_t), 0, addr, ADRSZ);
		goto recvryh;
	}
	rt_task_suspend(task);
}

static void port_server_fun(RT_TASK *port_server)
{
	short recovered;
	long i, rsize;
	RT_TASK *task;
	struct sockaddr *addr;
	struct req_rel_msg msg;

	addr = (struct sockaddr *)&portslot[0].addr;
	sprintf(current->comm, "PRTSRV");

	while (soft_rt_fun_call(port_server, rt_sem_wait, &portslot[0].sem) < RTE_LOWERR) {
		if (recovery.out != recovery.in) {
			recovered = 1;
			msg.priority = recovery.msg[recovery.out].priority;
			msg.hard = recovery.msg[recovery.out].hard;
			msg.owner = recovery.msg[recovery.out].owner;
			msg.name = TSK_FRM_WNR(recovery.msg[recovery.out].owner);
			*addr = recovery.msg[recovery.out].addr;
			msg.op = 0;
			msg.port = 0;
			recovery.out = (recovery.out + 1) & MaxStubsMone;
		} else {
			recovered = 0;
			rsize = soft_rt_recvfrom(portslot[0].socket[0], &msg, sizeof(msg), 0, addr, &i);
			if (decode) {
				decode(&portslot[0], &msg, rsize, PRT_SRV);
			}
		}
		if (msg.op) {
			msg.port = gvb_stub(msg.op - BASEPORT, msg.owner);
			goto ret;
		}
		if (!(msg.port = get_stub(msg.owner))) {
			msg.port = -ENODEV;
			goto ret;
		}
		if (!portslot[msg.port].task) {
			if ((task = kmalloc(sizeof(RT_TASK) + 2*sizeof(struct fun_args), GFP_KERNEL))) {
				if ((msg.hard ? rt_task_init(task, (void *)hard_stub_fun, (long)(portslot + msg.port), StackSize + 2*MAX_MSG_SIZE, msg.priority, 0, NULL) : soft_kthread_init(task, (long)soft_stub_fun, (long)(portslot + msg.port), msg.priority < BASE_SOFT_PRIORITY ? msg.priority + BASE_SOFT_PRIORITY : msg.priority))) {
					kfree(task);
					task = 0;
				}
			}
			if (!task) {
				portslot[msg.port].owner = 0;
				msg.port = -ENOMEM;
				goto ret;
			}
			portslot[msg.port].name = msg.name;
			portslot[msg.port].task = (long)task;
			if (msg.hard) {
				portslot[msg.port].hard = MSG_HARD;
			} else {
				portslot[msg.port].hard = MSG_SOFT;
			}
			portslot[msg.port].sem.count = 0;
			portslot[msg.port].sem.queue.prev = portslot[msg.port].sem.queue.next = &portslot[msg.port].sem.queue;
			portslot[msg.port].addr = portslot[0].addr;
			rt_task_resume(task);
		}
		portslot[msg.port].recovered = recovered;
		msg.rem_node = this_node[msg.hard];
		msg.port += BASEPORT;
		msg.chkspare = sizeof(long);
ret:
		if (recovered) {
			rt_task_masked_unblock((RT_TASK *)portslot[msg.port-BASEPORT].task,~RT_SCHED_READY);
		} else {
			soft_rt_sendto(portslot[0].socket[0], &msg, encode ? encode(&portslot[0], &msg, sizeof(msg), PRT_RTR) : sizeof(msg), 0, addr, ADRSZ);
		}
	}
//soft_rt_fun_call(port_server, rt_task_suspend, port_server);
}

static int mod_timer_srq;

RTAI_SYSCALL_MODE int rt_send_req_rel_port(unsigned long node, int op, unsigned long id, MBX *mbx, int hard)
{
	RT_TASK *task;
	long i, msgsize;
	struct portslot_t *portslotp;
	struct req_rel_msg msg;

	op >>= PORT_SHF;
	if (!node || (op && (op < MaxStubs || op >= MaxSocks))) {
		return -EINVAL;
	}
	if (!(portslotp = get_portslot())) {
		return -ENODEV;
	}
	portslotp->name = PRTSRVNAME;
	portslotp->addr = SPRT_ADDR;
	portslotp->addr.sin_addr.s_addr = node;
	task = _rt_whoami();
	if (op) {
		msg.op = ntohs(portslot[op].addr.sin_port);
		id = portslot[op].name;
		hard = portslot[op].hard;
	} else {
		msg.op = 0;
		if (!id) {
			id = (unsigned long)task;
		}
	}
	msg.port = portslotp->sem.count = 0;
	portslotp->sem.queue.prev = portslotp->sem.queue.next = &portslotp->sem.queue;
	msg.hard = hard ? MSG_HARD : MSG_SOFT;
	msg.name = id;
	msg.owner = OWNER(this_node[0], id);
	msg.rem_node = 0;
	msg.priority = task->base_priority;
	msgsize = encode ? encode(&portslot[0], &msg, sizeof(msg), PRT_REQ) : sizeof(msg);
	for (i = 0; i < NETRPC_TIMER_FREQ && !portslotp->sem.count; i++) {
		soft_rt_sendto(portslotp->socket[0], &msg, msgsize, 0, (void *)&portslotp->addr, ADRSZ);
		rt_pend_linux_srq(mod_timer_srq);
		rt_sem_wait(&timer_sem);
	}
	if (portslotp->sem.count >= 1) {
		msgsize = soft_rt_recvfrom(portslotp->socket[0], &msg, sizeof(msg), 0, (void *)&portslotp->addr, &i);
		if (decode) {
			decode(&portslot[0], &msg, msgsize, PRT_RCV);
		}
		if (msg.port > 0) {
			if (op) {
				portslot[op].task = 0;
				gvb_portslot(portslot + op);
				gvb_portslot(portslotp);
				return op;
			} else {
				check_portslot(node, msg.port, &portslotp);
				portslotp->sem.count = 0;
				portslotp->sem.queue.prev = portslotp->sem.queue.next = &portslotp->sem.queue;
				portslotp->hard = msg.hard;
				portslotp->owner = msg.owner;
				portslotp->name = msg.name;
				portslotp->addr.sin_port = htons(msg.port);
				portslotp->mbx  = mbx;
				portslotp->recovered = 1;
				portslotp->addr.sin_addr.s_addr = msg.rem_node;
				if (msg.chkspare == 4) {
					return (portslotp->indx << PORT_SHF);
				} else {
					return (portslotp->indx << PORT_SHF) + PORT_INC;
				}
			}
		}
	}
	gvb_portslot(portslotp);
	return msg.port ? msg.port : -ETIMEDOUT;
}

RTAI_SYSCALL_MODE int rt_set_netrpc_timeout(int port, RTIME timeout)
{
	portslot[port >> PORT_SHF].timeout = timeout;
	return 0;
}

RTAI_SYSCALL_MODE RT_TASK *rt_find_asgn_stub(unsigned long long owner, int asgn)
{
	int i;
	i = asgn ? get_stub(owner) : find_stub(owner);
	return i > 0 ? (RT_TASK *)portslot[i].task : 0;
}

RTAI_SYSCALL_MODE int rt_rel_stub(unsigned long long owner)
{
	int i;
	i = find_stub(owner);
	if (i)
	{
	i = gvb_stub(i,owner);
	return i;
	} else {
	return -ESRCH;
	}
}

RTAI_SYSCALL_MODE int rt_waiting_return(unsigned long node, int port)
{
	struct portslot_t *portslotp;
	portslotp = portslot + (abs(port) >> PORT_SHF);
	return portslotp->task < 0 && !portslotp->sem.count;
}

#if 0

static inline void mbx_send_if(MBX *mbx, void *sendmsg, int msg_size)
{
#define MOD_SIZE(indx) ((indx) < mbx->size ? (indx) : (indx) - mbx->size)

	unsigned long flags;
	int tocpy, avbs;
	char *msg;

	if (!mbx) {
		return;
	}
	msg = sendmsg;
	if (msg_size <= mbx->frbs) {
		RT_TASK *task;
		avbs = mbx->avbs;
		while (msg_size > 0 && mbx->frbs) {
			if ((tocpy = mbx->size - mbx->lbyte) > msg_size) {
				tocpy = msg_size;
			}
			if (tocpy > mbx->frbs) {
				tocpy = mbx->frbs;
			}
			memcpy(mbx->bufadr + mbx->lbyte, msg, tocpy);
			flags = rt_spin_lock_irqsave(&mbx->lock);
			mbx->frbs -= tocpy;
			rt_spin_unlock_irqrestore(flags, &mbx->lock);
			avbs += tocpy;
			msg_size -= tocpy;
			*msg += tocpy;
			mbx->lbyte = MOD_SIZE(mbx->lbyte + tocpy);
		}
		mbx->avbs = avbs;
		flags = rt_global_save_flags_and_cli();
		if ((task = mbx->waiting_task)) {
			rem_timed_task(task);
			mbx->waiting_task = (void *)0;
			if (task->state != RT_SCHED_READY && (task->state &= ~(RT_SCHED_MBXSUSP | RT_SCHED_DELAYED)) == RT_SCHED_READY) {
				enq_ready_task(task);
				rt_schedule();
			}
		}
		rt_global_restore_flags(flags);
	}
}

#else

static inline void mbx_signal(MBX *mbx)
{
	unsigned long flags;
	RT_TASK *task;

	flags = rt_global_save_flags_and_cli();
	if ((task = mbx->waiting_task)) {
		rem_timed_task(task);
		task->blocked_on  = NULL;
		mbx->waiting_task = NULL;
		if (task->state != RT_SCHED_READY && (task->state &= ~(RT_SCHED_MBXSUSP | RT_SCHED_DELAYED)) == RT_SCHED_READY) {
			enq_ready_task(task);
			RT_SCHEDULE(task, rtai_cpuid());
		}
	}
	rt_global_restore_flags(flags);
}

#define MOD_SIZE(indx) ((indx) < mbx->size ? (indx) : (indx) - mbx->size)

static inline void mbxput(MBX *mbx, char **msg, int msg_size)
{
	unsigned long flags;
	int tocpy;

	while (msg_size > 0 && mbx->frbs) {
		if ((tocpy = mbx->size - mbx->lbyte) > msg_size) {
			tocpy = msg_size;
		}
		if (tocpy > mbx->frbs) {
			tocpy = mbx->frbs;
		}
		memcpy(mbx->bufadr + mbx->lbyte, *msg, tocpy);
		flags = rt_spin_lock_irqsave(&(mbx->lock));
		mbx->frbs -= tocpy;
		mbx->avbs += tocpy;
		rt_spin_unlock_irqrestore(flags, &(mbx->lock));
		msg_size -= tocpy;
		*msg     += tocpy;
		mbx->lbyte = MOD_SIZE(mbx->lbyte + tocpy);
	}
}

static void mbx_send_if(MBX *mbx, void *msg, int msg_size)
{
	unsigned long flags;
	RT_TASK *rt_current;

	if (!mbx || mbx->magic != RT_MBX_MAGIC) {
		return;
	}

	flags = rt_global_save_flags_and_cli();
	rt_current = RT_CURRENT;
	if (mbx->sndsem.count > 0 && msg_size <= mbx->frbs) {
		mbx->sndsem.count = 0;
		if (mbx->sndsem.type > 0) {
			mbx->sndsem.owndby = rt_current;
			enqueue_resqel(&mbx->sndsem.resq, rt_current);
		}
		rt_global_restore_flags(flags);
		mbxput(mbx, (char **)(&msg), msg_size);
		mbx_signal(mbx);
		rt_sem_signal(&mbx->sndsem);
	}
	rt_global_restore_flags(flags);
}

#endif

#define RETURN_ERR(err) \
	do { \
		union { long long ll; long l; } retval; \
		retval.l = err; \
		return retval.ll; \
	} while (0)

RTAI_SYSCALL_MODE long long _rt_net_rpc(long fun_ext_timed, long type, void *args, int argsize, int space, unsigned long partypes)
{
	char msg[MAX_MSG_SIZE];
	struct reply_t *reply;
	long rsize, port;
	struct portslot_t *portslotp;

	if ((port = PORT(fun_ext_timed)) > 0) {
		port >>= PORT_SHF;
		if ((portslotp = portslot + port)->task < 0) {
			long i;
			struct sockaddr addr;

			if (portslotp->timeout) {
				if(rt_sem_wait_timed(&portslotp->sem,portslotp->timeout) == RTE_TIMOUT)
					RETURN_ERR(RTE_NETIMOUT);
			} else {
				rt_sem_wait(&portslotp->sem);
			}
			if ((rsize = portslotp->hard ? hard_rt_recvfrom(portslotp->socket[1], msg, MAX_MSG_SIZE, 0, &addr, (void *)&i) : soft_rt_recvfrom(portslotp->socket[0], msg, MAX_MSG_SIZE, 0, &addr, &i))) {
				if (decode) {
					rsize = decode(portslotp, msg, rsize, RPC_RCV);
				}
				if((reply = (void *)msg)->myport) {
					if (reply->myport < 0) {
						RETURN_ERR(-RTE_CHGPORTERR);
					}
					portslotp->addr.sin_port = htons(reply->myport);
					portslotp->sem.count = 0;
					portslotp->sem.queue.prev = portslotp->sem.queue.next = &portslotp->sem.queue;
					portslotp->owner = reply->retval;
					portslotp->name = (unsigned long)(_rt_whoami());
					RETURN_ERR(-RTE_CHGPORTOK);
				}
				mbx_send_if(portslotp->mbx, msg, offsetof(struct reply_t, msg) + reply->wsize + reply->w2size);
//				mbx_send_if(portslotp->mbx, msg, rsize);
			}
			portslotp->task = 1;
		}
		portslotp->msg = msg;
	} else {
		port = -(abs(port) >> PORT_SHF);
		if ((portslotp = portslot - port)->task < 0) {
			if (!rt_sem_wait_if(&portslotp->sem)) {
				return 0;
			} else {
				long i;
				struct sockaddr addr;

				if ((rsize = portslotp->hard ? hard_rt_recvfrom(portslotp->socket[1], msg, MAX_MSG_SIZE, 0, &addr, (void *)&i) : soft_rt_recvfrom(portslotp->socket[0], msg, MAX_MSG_SIZE, 0, &addr, &i))) {
					if (decode) {
						rsize = decode(portslotp, msg, rsize, RPC_RCV);
					}
						if((reply = (void *)msg)->myport) {
							if (reply->myport < 0) {
								RETURN_ERR(-RTE_CHGPORTERR);
							}

								portslotp->addr.sin_port = htons(reply->myport);
								portslotp->sem.count = 0;
								portslotp->sem.queue.prev = portslotp->sem.queue.next = &portslotp->sem.queue;
								portslotp->owner = reply->retval;
								portslotp->name = (unsigned long)(_rt_whoami());
								RETURN_ERR(-RTE_CHGPORTOK);
						}
					mbx_send_if(portslotp->mbx, msg, offsetof(struct reply_t, msg) + reply->wsize + reply->w2size);
//					mbx_send_if(portslotp->mbx, msg, rsize);
				}
			}
		} else {
			portslotp->task = -1;
		}
	}
	if (FUN(fun_ext_timed) == SYNC_NET_RPC) {
		RETURN_ERR(1);
	}
	if (NEED_TO_R(type)) {
		rsize = USP_RSZ1(type);
		rsize = rsize ? ((long *)args)[rsize - 1] : sizeof(long);
	} else {
		rsize = 0;
	}
	do {
		struct par_t *arg;
		RT_TASK *task;

		arg = (void *)msg;
		arg->priority = (task = _rt_whoami())->priority;
		arg->base_priority = task->base_priority;
		arg->argsize = argsize;
		arg->rsize = rsize;
		arg->fun_ext_timed = fun_ext_timed;
		arg->type = type;
		arg->owner = portslotp->owner;
		arg->partypes = partypes;
		memcpy(arg->a, args, argsize);
		if (rsize > 0) {
			if (space) {
				memcpy((char *)arg->a + argsize, (void *)((long *)args + USP_RBF1(type) - 1)[0], rsize);
			} else {
				rt_copy_from_user((char *)arg->a + argsize, (void *)((long *)args + USP_RBF1(type) - 1)[0], rsize);
			}
		}
		rsize = sizeof(struct par_t) - sizeof(long) + argsize + rsize;
		if (encode) {
			rsize = encode(portslotp, msg, rsize, RPC_REQ);
		}
		arg->mach = sizeof(long);
		if (portslotp->hard) {
			hard_rt_sendto(portslotp->socket[1], msg, rsize, 0, (struct sockaddr *)&portslotp->addr, ADRSZ);
		} else  {
			soft_rt_sendto(portslotp->socket[0], msg, rsize, 0, (struct sockaddr *)&portslotp->addr, ADRSZ);
		}
	} while (0);
	if (port > 0) {
		struct sockaddr addr;

		if (portslotp->timeout) {
			if(rt_sem_wait_timed(&portslotp->sem,portslotp->timeout) == RTE_TIMOUT)
				RETURN_ERR(-RTE_NETIMOUT);
		} else {
			rt_sem_wait(&portslotp->sem);
		}
		rsize = portslotp->hard ? hard_rt_recvfrom(portslotp->socket[1], msg, MAX_MSG_SIZE, 0, &addr, (void *)&port) : soft_rt_recvfrom(portslotp->socket[0], msg, MAX_MSG_SIZE, 0, &addr, &port);
		if (decode) {
			decode(portslotp, portslotp->msg, rsize, RPC_RCV);
		}
		if((reply = (void *)msg)->myport) {
			if (reply->myport < 0) {
				RETURN_ERR(-RTE_CHGPORTERR);
			}
			portslotp->addr.sin_port = htons(reply->myport);
			portslotp->sem.count = 0;
			portslotp->sem.queue.prev = portslotp->sem.queue.next = &portslotp->sem.queue;
			portslotp->owner = reply->retval;
			portslotp->name = (unsigned long)(_rt_whoami());
			RETURN_ERR(-RTE_CHGPORTOK);
		} else {
			if (reply->wsize) {
				if (space) {
					memcpy((char *)(*((long *)args + USP_WBF1(type) - 1)), reply->msg, reply->wsize);
				} else {
					rt_copy_to_user((char *)(*((long *)args + USP_WBF1(type) - 1)), reply->msg, reply->wsize);
				}
				if (reply->w2size) {
					if (space) {
						memcpy((char *)(*((long *)args + USP_WBF2(type) - 1)), reply->msg + reply->wsize, reply->w2size);
					} else {
						rt_copy_to_user((char *)(*((long *)args + USP_WBF2(type) - 1)), reply->msg + reply->wsize, reply->w2size);
					}
				}
			}
			return reply->retval;
		}
	}
	return 0;
}

int rt_get_net_rpc_ret(MBX *mbx, unsigned long long *retval, void *msg1, int *msglen1, void *msg2, int *msglen2, RTIME timeout, int type)
{
	struct reply_t reply;
	int ret;

	if ((ret = ((int (*)(MBX *, ...))rt_net_rpc_fun_ext[NET_RPC_EXT][type].fun)(mbx, &reply, offsetof(struct reply_t, msg), timeout))) {
		return ret;
	}
	*retval = reply.retval;
	if (reply.wsize) {
		if (*msglen1 > reply.wsize) {
			*msglen1 = reply.wsize;
		}
		_rt_mbx_receive(mbx, &msg1, *msglen1, 1);
	} else {
		*msglen1 = 0;
	}
	if (reply.w2size) {
		if (*msglen2 > reply.w2size) {
			*msglen2 = reply.w2size;
		}
		_rt_mbx_receive(mbx, &msg2, *msglen2, 1);
	} else {
		*msglen2 = 0;
	}
	return 0;
}

RTAI_SYSCALL_MODE unsigned long ddn2nl(const char *ddn)
{
	int p, n, c;
	union { unsigned long l; char c[4]; } u;

	p = n = 0;
	while ((c = *ddn++)) {
		if (c != '.') {
			n = n*10 + c - '0';
		} else {
			if (n > 0xFF) {
				return 0;
			}
			u.c[p++] = n;
			n = 0;
		}
	}
	u.c[3] = n;

	return u.l;
}

RTAI_SYSCALL_MODE unsigned long rt_set_this_node(const char *ddn, unsigned long node, int hard)
{
	return this_node[hard ? MSG_HARD : MSG_SOFT] = ddn ? ddn2nl(ddn) : node;
}

#ifdef CONFIG_RTAI_RT_POLL

RTAI_SYSCALL_MODE int rt_poll_netrpc(struct rt_poll_s *pdsa1, struct rt_poll_s *pdsa2, unsigned long pdsa_size, RTIME timeout)
{
	int retval = pdsa_size/sizeof(struct rt_poll_s);
	if (sizeof(long) == 8 && !((unsigned long)pdsa1[0].what & 0xFFFFFFFF00000000ULL)) {
		int i;
		for (i = 0; i < retval; i++) {
			pdsa1[i].what = (void *)reset_kadr((unsigned long)pdsa1[i].what);
		}
	}
	retval = _rt_poll(pdsa1, retval, timeout, 1);
	memcpy(pdsa2, pdsa1, pdsa_size);
	return retval;
}

EXPORT_SYMBOL(rt_poll_netrpc);

#endif

/* +++++++++++++++++++++++++++ NETRPC ENTRIES +++++++++++++++++++++++++++++++ */

struct rt_native_fun_entry rt_netrpc_entries[] = {
	{ { 1, _rt_net_rpc           },  NETRPC },
	{ { 0, rt_set_netrpc_timeout },	 SET_NETRPC_TIMEOUT },
	{ { 1, rt_send_req_rel_port  },	 SEND_REQ_REL_PORT },
	{ { 0, ddn2nl                },	 DDN2NL },
	{ { 0, rt_set_this_node      },	 SET_THIS_NODE },
	{ { 0, rt_find_asgn_stub     },	 FIND_ASGN_STUB },
	{ { 0, rt_rel_stub           },	 REL_STUB },
	{ { 0, rt_waiting_return     },	 WAITING_RETURN },
#ifdef CONFIG_RTAI_RT_POLL
	{ { 1, rt_poll_netrpc        },  RT_POLL_NETRPC },
#endif
	{ { 0, 0 },                    	000 }
};

extern int set_rt_fun_entries(struct rt_native_fun_entry *entry);
extern void reset_rt_fun_entries(struct rt_native_fun_entry *entry);

static RT_TASK *port_server;

static int init_softrtnet(void);
static void cleanup_softrtnet(void);

void do_mod_timer(void)
{
	mod_timer(&timer, jiffies + HZ/NETRPC_TIMER_FREQ);
}

static struct sock_t *socks;

int soft_rt_socket(int domain, int type, int protocol)
{
	int i;
	for (i = 0; i < MaxSocks; i++) {
		if (!cmpxchg(&socks[i].opnd, 0, 1)) {
			return i;
		}
	}
	return -1;
}

int soft_rt_close(int sock)
{
	if (sock >= 0 && sock < MaxSocks) {
		return socks[sock].opnd = 0;
	}
	return -1;
}

int soft_rt_bind(int sock, struct sockaddr *addr, int addrlen)
{
	return 0;
}

int soft_rt_socket_callback(int sock, int (*func)(int sock, void *arg), void *arg)
{
	if (sock >= 0 && sock < MaxSocks && func > 0) {
		socks[sock].callback = func;
		socks[sock].arg      = arg;
		return 0;
	}
	return -1;
}

static int MaxSockSrq;
static struct { int srq, in, out, *sockindx; } sysrq;
static DEFINE_SPINLOCK(sysrq_lock);

int soft_rt_sendto(int sock, const void *msg, int msglen, unsigned int sflags, struct sockaddr *to, int tolen)
{
	unsigned long flags;
	if (sock >= 0 && sock < MaxSocks) {
		if (msglen > MAX_MSG_SIZE) {
			msglen = MAX_MSG_SIZE;
		}
		memcpy(socks[sock].msg, msg, socks[sock].tosend = msglen);
		memcpy(&socks[sock].addr, to, tolen);
		flags = rt_spin_lock_irqsave(&sysrq_lock);
		sysrq.sockindx[sysrq.in] = sock;
		sysrq.in = (sysrq.in + 1) & MaxSockSrq;
		rt_spin_unlock_irqrestore(flags, &sysrq_lock);
		rt_pend_linux_srq(sysrq.srq);
		return msglen;
	}
	return -1;
}

int soft_rt_recvfrom(int sock, void *msg, int msglen, unsigned int flags, struct sockaddr *from, long *fromlen)
{
	if (sock >= 0 && sock < MaxSocks) {
		if (msglen > socks[sock].recvd) {
			msglen = socks[sock].recvd;
		}
		memcpy(msg, socks[sock].msg, msglen);
		if (from && fromlen) {
			memcpy(from, &socks[sock].addr, socks[sock].addrlen);
			*fromlen = socks[sock].addrlen;
		}
		return msglen;
	}
	return -1;
}

#include <linux/unistd.h>
#include <linux/poll.h>
#include <linux/net.h>

int errno;

#define SYSCALL_BGN() \
	do { int retval; mm_segment_t svdfs = get_fs(); set_fs(KERNEL_DS)
#define SYSCALL_END() \
	set_fs(svdfs); return retval; } while (0)

#ifdef __NR_socketcall

extern void *sys_call_table[];

//static _syscall3(int, poll, struct pollfd *, ufds, unsigned int, nfds, int, timeout)
static inline int kpoll(struct pollfd *ufds, unsigned int nfds, int timeout)
{
	SYSCALL_BGN();
	retval = ((asmlinkage int (*)(struct pollfd *, ... ))sys_call_table[__NR_poll])(ufds, nfds, timeout);
//	retval = poll(ufds, nfds, timeout);
	SYSCALL_END();
}

//static _syscall2(int, socketcall, int, call, void *, args)
static inline int ksocketcall(int call, void *args)
{
	SYSCALL_BGN();
	retval = ((asmlinkage int (*)(int, ... ))sys_call_table[__NR_socketcall])(call, args);
//	retval = socketcall(call, args);
	SYSCALL_END();
}

static inline int ksocket(int family, int type, int protocol)
{
	struct { int family; int type; int protocol; } args = { family, type, protocol };
	return ksocketcall(SYS_SOCKET, &args);
}

static inline int kbind(int fd, struct sockaddr *umyaddr, int addrlen)
{
	struct { int fd; struct sockaddr *umyaddr; int addrlen; } args = { fd, umyaddr, addrlen };
	return ksocketcall(SYS_BIND, &args);
}

static inline int kconnect(int fd, struct sockaddr *serv_addr, int addrlen)
{
	struct { int fd; struct sockaddr *serv_addr; int addrlen; } args = { fd, serv_addr, addrlen };
	return ksocketcall(SYS_CONNECT, &args);
}

static inline int klisten(int fd, int backlog)
{
	struct { int fd; int backlog; } args = { fd, backlog };
	return ksocketcall(SYS_LISTEN, &args);
}

static inline int kaccept(int fd, struct sockaddr *upeer_sockaddr, int *upeer_addrlen)
{
	struct { int fd; struct sockaddr *upeer_sockaddr; int *upeer_addrlen; } args = { fd, upeer_sockaddr, upeer_addrlen };
	return ksocketcall(SYS_ACCEPT, &args);
}

static inline int kgetsockname(int fd, struct sockaddr *usockaddr, int *usockaddr_len)
{
	struct { int fd; struct sockaddr *usockaddr; int *usockaddr_len; } args = { fd, usockaddr, usockaddr_len };
	return ksocketcall(SYS_GETSOCKNAME, &args);
}

static inline int kgetpeername(int fd, struct sockaddr *usockaddr, int *usockaddr_len)
{
	struct { int fd; struct sockaddr *usockaddr; int *usockaddr_len; } args = { fd, usockaddr, usockaddr_len };
	return ksocketcall(SYS_GETPEERNAME, &args);
}

static inline int ksocketpair(int family, int type, int protocol, int *usockvec)
{
	struct { int family; int type; int protocol; int *usockvec; } args = { family, type, protocol, usockvec };
	return ksocketcall(SYS_SOCKETPAIR, &args);
}

static inline int ksendto(int fd, void *buff, size_t len, unsigned flags, struct sockaddr *addr, int addr_len)
{
	struct { int fd; void *buff; size_t len; unsigned flags; struct sockaddr *addr; int addr_len; } args = { fd, buff, len, flags, addr, addr_len };
	return ksocketcall(SYS_SENDTO, &args);
}

static inline int krecvfrom(int fd, void *ubuf, size_t len, unsigned flags, struct sockaddr *addr, int *addr_len)
{
	struct { int fd; void *ubuf; size_t len; unsigned flags; struct sockaddr *addr; int *addr_len; } args = { fd, ubuf, len, flags, addr, addr_len };
	return ksocketcall(SYS_RECVFROM, &args);
}

static inline int kshutdown(int fd, int how)
{
	struct { int fd; int how; } args = { fd, how };
	return ksocketcall(SYS_SHUTDOWN, &args);
}

static inline int ksetsockopt(int fd, int level, int optname, void *optval, int optlen)
{
	struct { int fd; int level; int optname; void *optval; int optlen; } args = { fd, level, optname, optval, optlen };
	return ksocketcall(SYS_SETSOCKOPT, &args);
}

static inline int kgetsockopt(int fd, int level, int optname, char *optval, int *optlen)
{
	struct { int fd; int level; int optname; void *optval; int *optlen; } args = { fd, level, optname, optval, optlen };
	return ksocketcall(SYS_GETSOCKOPT, &args);
}

static inline int ksendmsg(int fd, struct msghdr *msg, unsigned flags)
{
	struct { int fd; struct msghdr *msg; unsigned flags; } args = { fd, msg, flags };
	return ksocketcall(SYS_SENDMSG, &args);
}

static inline int krecvmsg(int fd, struct msghdr *msg, unsigned flags)
{
	struct { int fd; struct msghdr *msg; unsigned flags; } args = { fd, msg, flags };
	return ksocketcall(SYS_RECVMSG, &args);
}

#else

#if 0  // just for compiling
#define __NR_socket       1
#define __NR_bind         1
#define __NR_connect      1
#define __NR_accept       1
#define __NR_listen       1
#define __NR_getsockname  1
#define __NR_getpeername  1
#define __NR_socketpair   1
#define __NR_sendto       1
#define __NR_recvfrom     1
#define __NR_shutdown     1
#define __NR_setsockopt   1
#define __NR_getsockopt   1
#define __NR_sendmsg      1
#define __NR_recvmsg      1
#endif

extern void *sys_call_table[];

//static _syscall3(int, poll, struct pollfd *, ufds, unsigned int, nfds, int, timeout)
static inline int kpoll(struct pollfd *ufds, unsigned int nfds, int timeout)
{
	SYSCALL_BGN();
	retval = ((int (*)(void *, unsigned int, int))sys_call_table[__NR_poll])(ufds, nfds, timeout);
//	retval = poll(ufds, nfds, timeout);
	SYSCALL_END();
}

//static _syscall3(int, socket, int, family, int, type, int, protocol)
static inline int ksocket(int family, int type, int protocol)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, int, int))sys_call_table[__NR_socket])(family, type, protocol);
//	retval = socket(family, type, protocol);
	SYSCALL_END();
}

//static _syscall3(int, bind, int, fd, struct sockaddr *, umyaddr, int, addrlen)
static inline int kbind(int fd, struct sockaddr *umyaddr, int addrlen)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, struct sockaddr *, int))sys_call_table[__NR_bind])(fd, umyaddr, addrlen);
//	retval = bind(fd, umyaddr, addrlen);
	SYSCALL_END();
}

//static _syscall3(int, connect, int, fd, struct sockaddr *, serv_addr, int, addrlen)
static inline int kconnect(int fd, struct sockaddr *serv_addr, int addrlen)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, struct sockaddr *, int))sys_call_table[__NR_connect])(fd, serv_addr, addrlen);
//	retval = connect(fd, serv_addr, addrlen);
	SYSCALL_END();
}

//static _syscall2(int, listen, int, fd, int, backlog)
static inline int klisten(int fd, int backlog)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, int))sys_call_table[__NR_listen])(fd, backlog);
//	retval = listen(fd, backlog);
	SYSCALL_END();
}

//static _syscall3(int, accept, int, fd, struct sockaddr *, upeer_sockaddr, int *, upeer_addrlen)
static inline int kaccept(int fd, struct sockaddr *upeer_sockaddr, int *upeer_addrlen)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, struct sockaddr *, int *))sys_call_table[__NR_accept])(fd, upeer_sockaddr, upeer_addrlen);
//	retval = accept(fd, upeer_sockaddr, upeer_addrlen);
	SYSCALL_END();
}

//static _syscall3(int, getsockname, int, fd, struct sockaddr *, usockaddr, int *, uaddr_len)
static inline int kgetsockname(int fd, struct sockaddr *usockaddr, int *usockaddr_len)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, struct sockaddr *, int *))sys_call_table[__NR_getsockname])(fd, usockaddr, usockaddr_len);
//	retval = getsockname(fd, usockaddr, usockaddr_len);
	SYSCALL_END();
}

//static _syscall3(int, getpeername, int, fd, struct sockaddr *, usockaddr, int *, uaddr_len)
static inline int kgetpeername(int fd, struct sockaddr *usockaddr, int *usockaddr_len)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, struct sockaddr *, int *))sys_call_table[__NR_getpeername])(fd, usockaddr, usockaddr_len);
//	retval = getpeername(fd, usockaddr, usockaddr_len);
	SYSCALL_END();
}

//static _syscall4(int, socketpair, int, family, int, type, int, protocol, int, usockvec[2])
static inline int ksocketpair(int family, int type, int protocol, int *usockvec)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, int, int, int *))sys_call_table[__NR_socketpair])(family, type, protocol, usockvec);
//	retval = socketpair(family, type, protocol, usockvec);
	SYSCALL_END();
}

//static _syscall6(int, sendto, int, fd, void *, ubuf, size_t, len, unsigned, flags, struct sockaddr *, addr, int, addr_len)
static inline int ksendto(int fd, void *buff, size_t len, unsigned flags, struct sockaddr *addr, int addr_len)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, void *, size_t, unsigned, struct sockaddr *, int))sys_call_table[__NR_sendto])(fd, buff, len, flags, addr, addr_len);
//	retval = sendto(fd, buff, len, flags, addr, addr_len);
	SYSCALL_END();
}

//static _syscall6(int, recvfrom, int, fd, void *, ubuf, size_t, len, unsigned, flags, struct sockaddr *, addr, int *, addr_len)
static inline int krecvfrom(int fd, void *ubuf, size_t len, unsigned flags, struct sockaddr *addr, int *addr_len)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, void *, size_t, unsigned, struct sockaddr *, int *))sys_call_table[__NR_recvfrom])(fd, ubuf, len, flags, addr, addr_len);
//	retval = recvfrom(fd, ubuf, len, flags, addr, addr_len);
	SYSCALL_END();
}

//static _syscall2(int, shutdown, int, fd, int, how)
static inline int kshutdown(int fd, int how)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, int))sys_call_table[__NR_shutdown])(fd, how);
//	retval = shutdown(fd, how);
	SYSCALL_END();
}

//static _syscall5(int, setsockopt, int, fd, int, level, int, optname, void *, optval, int, optlen)
static inline int ksetsockopt(int fd, int level, int optname, void *optval, int optlen)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, int, int, void *, int))sys_call_table[__NR_setsockopt])(fd, level, optname, optval, optlen);
//	retval = setsockopt(fd, level, optname, optval, optlen);
	SYSCALL_END();
}

//static _syscall5(int, getsockopt, int, fd, int, level, int, optname, char *, optval, int *, optlen)
static inline int kgetsockopt(int fd, int level, int optname, char *optval, int *optlen)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, int, int, void *, int *))sys_call_table[__NR_getsockopt])(fd, level, optname, optval, optlen);
//	retval = getsockopt(fd, level, optname, optval, optlen);
	SYSCALL_END();
}

//static _syscall3(int, sendmsg, int, fd, struct msghdr *, msg, unsigned, flags)
static inline int ksendmsg(int fd, struct msghdr *msg, unsigned flags)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, struct msghdr *, unsigned))sys_call_table[__NR_sendmsg])(fd, msg, flags);
//	retval = sendmsg(fd, msg, flags);
	SYSCALL_END();
}

//static _syscall3(int, recvmsg, int, fd, struct msghdr *, msg, unsigned, flags)
static inline int krecvmsg(int fd, struct msghdr *msg, unsigned flags)
{
	SYSCALL_BGN();
	retval = ((int (*)(int, struct msghdr *, unsigned))sys_call_table[__NR_recvmsg])(fd, msg, flags);
//	retval = recvmsg(fd, msg, flags);
	SYSCALL_END();
}

#endif

static inline int ksend(int fd, void *buff, size_t len, unsigned flags)
{
	return ksendto(fd, buff, len, flags, NULL, 0);
}

static inline int krecv(int fd, void *ubuf, size_t size, unsigned flags)
{
	return krecvfrom(fd, ubuf, size, flags, NULL, NULL);
}

static DECLARE_MUTEX_LOCKED(mtx);
static unsigned long end_softrtnet;

static void send_thread(void)
{
	int i;

	sprintf(current->comm, "SNDSRV");
	rtai_set_linux_task_priority(current,SCHED_FIFO,MAX_LINUX_RTPRIO);
	sigfillset(&current->blocked);
	while (!end_softrtnet) {
		i = down_interruptible(&mtx);
		while (sysrq.out != sysrq.in) {
			i = sysrq.sockindx[sysrq.out];
			ksendto(socks[i].sock, socks[i].msg, socks[i].tosend, MSG_DONTWAIT, &socks[i].addr, ADRSZ);
			sysrq.out = (sysrq.out + 1) & MaxSockSrq;
		}
	}
	set_bit(1, &end_softrtnet);
}

static struct pollfd *pollv;
static struct task_struct *recv_handle;

static void recv_thread(void)
{
	int i, nevents;

	recv_handle = current;
	sprintf(current->comm, "RCVSRV");
	rtai_set_linux_task_priority(current,SCHED_RR,MAX_LINUX_RTPRIO);
	sigfillset(&current->blocked);
	while (!end_softrtnet) {
		if ((nevents = kpoll(pollv, MaxSocks, 1000)) > 0) {
			i = -1;
			do {
				while (!pollv[++i].revents);
				if ((socks[i].recvd = krecvfrom(socks[i].sock, socks[i].msg, MAX_MSG_SIZE, MSG_DONTWAIT, &socks[i].addr, &socks[i].addrlen)) > 0) {
					socks[i].callback(i, socks[i].arg);
				}
			} while (--nevents);
		}
	}
	set_bit(2, &end_softrtnet);
}

static void softrtnet_hdl(void)
{
	up(&mtx);
}

static int init_softrtnet(void)
{
	int i;
	for (i = 8*sizeof(unsigned long) - 1; !test_bit(i, &MaxSocks); i--);
	MaxSockSrq = ((1 << i) != MaxSocks ? 1 << (i + 1) : MaxSocks) - 1;
	if ((sysrq.srq = rt_request_srq(0xbadbeef2, softrtnet_hdl, 0)) < 0) {
		printk("SOFT RTNet: no sysrq available.\n");
		return sysrq.srq;
	}
	if (!(sysrq.sockindx = (int *)kmalloc((MaxSockSrq + 1)*sizeof(int), GFP_KERNEL))) {
		printk("SOFT RTNet: no memory available for socket queus.\n");
		return -ENOMEM;
	}
	if (!(socks = (struct sock_t *)kmalloc(MaxSocks*sizeof(struct sock_t), GFP_KERNEL))) {
		kfree(sysrq.sockindx);
		printk("SOFT RTNet: no memory available for socks.\n");
		return -ENOMEM;
	}
	if (!(pollv = (struct pollfd *)kmalloc(MaxSocks*sizeof(struct pollfd), GFP_KERNEL))) {
		kfree(sysrq.sockindx);
		kfree(socks);
		printk("SOFT RTNet: no memory available for polling.\n");
		return -ENOMEM;
	}
	memset(socks, 0, MaxSocks*sizeof(struct sock_t));
	for (i = 0; i < MaxSocks; i++) {
		SPRT_ADDR.sin_port = htons(BASEPORT + i);
		if ((socks[i].sock = ksocket(AF_INET, SOCK_DGRAM, 0)) < 0 || kbind(socks[i].sock, (struct sockaddr *)&SPRT_ADDR, ADRSZ) < 0) {
			rt_free_srq(sysrq.srq);
			kfree(socks);
			kfree(pollv);
			kfree(sysrq.sockindx);
			printk("SOFT RTNet: unable to set up Linux support sockets.\n");
			return -ESOCKTNOSUPPORT;
		}
		socks[i].addrlen = ADRSZ;
		pollv[i].fd     = socks[i].sock;
		pollv[i].events = POLLIN;
	}
	if (kernel_thread((void *)send_thread, 0, 0) <= 0 || kernel_thread((void *)recv_thread, 0, 0) <= 0) {
			kfree(sysrq.sockindx);
			kfree(socks);
			kfree(pollv);
			printk("SOFT RTNet: unable to set up Linux support kernel threads.\n");
			return -EINVAL;
	}
	while (!recv_handle) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(HZ/2);
	}
	return 0;
}

static void cleanup_softrtnet(void)
{
	int i;
	rt_free_srq(sysrq.srq);
	end_softrtnet = 1;
/* watch out: dirty trick, but we are sure the thread will do nothing more. */
//	sigemptyset(&recv_handle->blocked);
//	send_sig(SIGKILL, recv_handle, 1);
/* watch out: end of the dirty trick. */
	softrtnet_hdl();
	while (end_softrtnet < 7) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(HZ/2);
	}
	for (i = 0; i < MaxSocks; i++) {
		kshutdown(socks[i].sock, 2);
	}
	kfree(sysrq.sockindx);
	kfree(socks);
	kfree(pollv);
}

/*
 * this is a thing to make available externally what it should not,
 * needed to check the working of a user message processing addon
 */
void **rt_net_rpc_fun_hook = (void *)rt_net_rpc_fun_ext;

int __rtai_netrpc_init(void)
{
	int i;

	MaxStubsMone = MaxStubs - 1;
	if ((mod_timer_srq = rt_request_srq(0xbadbeef1, do_mod_timer, 0)) < 0) {
		printk("MOD_TIMER: no sysrq available.\n");
		return mod_timer_srq;
	}
	if (!(recovery.msg = (struct recovery_msg *)kmalloc((MaxStubs)*sizeof(struct recovery_msg), GFP_KERNEL))) {
		printk("Init MODULE no memory for recovery queue.\n");
		rt_free_srq(mod_timer_srq);
		return -ENOMEM;
	}
	MaxSocks += MaxStubs;
	SPRT_ADDR.sin_family = AF_INET;
	SPRT_ADDR.sin_addr.s_addr = htonl(INADDR_ANY);
	if (init_softrtnet()) {
		return 1;
	}
	rt_net_rpc_fun_ext[NET_RPC_EXT] = rt_fun_lxrt;
	set_rt_fun_entries(rt_netrpc_entries);
	if (!(portslot = kmalloc(MaxSocks*sizeof(struct portslot_t), GFP_KERNEL))) {
		printk("KMALLOC FAILED ALLOCATING PORT SLOTS\n");
	}
	if (!ThisSoftNode) {
		ThisSoftNode = ThisNode;
	}
	if (!ThisHardNode) {
		ThisHardNode = ThisNode;
	}
	this_node[0] = ddn2nl(ThisSoftNode);
	this_node[1] = ddn2nl(ThisHardNode);

	for (i = 0; i < MaxSocks; i++) {
		portslot[i].p = portslot + i;
		portslot[i].indx = portslot[i].place = i;
		SPRT_ADDR.sin_port = htons(BASEPORT + i);
		portslot[i].addr = SPRT_ADDR;
		portslot[i].socket[0] = soft_rt_socket(AF_INET, SOCK_DGRAM, 0);
		soft_rt_bind(portslot[i].socket[0], (struct sockaddr *)&SPRT_ADDR, ADRSZ);
		portslot[i].socket[1] = hard_rt_socket(AF_INET, SOCK_DGRAM, 0);
		hard_rt_bind(portslot[i].socket[1], (struct sockaddr *)&SPRT_ADDR, ADRSZ);
		soft_rt_socket_callback(portslot[i].socket[0], (void *)net_resume_task, &portslot[i].p);
		hard_rt_socket_callback(portslot[i].socket[1], (void *)net_resume_task, &portslot[i].p);
		portslot[i].owner = 0;
		rt_typed_sem_init(&portslot[i].sem, 0, BIN_SEM | FIFO_Q);
		portslot[i].task = 0;
		portslot[i].timeout = 0;
	}
	SPRT_ADDR.sin_port = htons(BASEPORT);
	portslotsp = MaxStubs;
	portslot[0].hard = 0;
	portslot[0].name = PRTSRVNAME;
	portslot[0].owner = OWNER(this_node[0], (unsigned long)port_server);
	port_server = kmalloc(sizeof(RT_TASK) + 3*sizeof(struct fun_args), GFP_KERNEL);
	soft_kthread_init(port_server, (long)port_server_fun, (long)port_server, RT_SCHED_LOWEST_PRIORITY);
	portslot[0].task = (long)port_server;
	rt_task_resume(port_server);
	rt_typed_sem_init(&timer_sem, 0, BIN_SEM | FIFO_Q);
	init_timer(&timer);
	timer.function = timer_fun;
	return 0 ;
}

void __rtai_netrpc_exit(void)
{
	int i;

	reset_rt_fun_ext_index(NULL, 1);
	if (encdec_ext) {
		reset_rt_fun_ext_index(encdec_ext, 1);
	}
	del_timer(&timer);
	rt_sem_delete(&timer_sem);
	for (i = 0; i < MaxStubs; i++) {
		if (portslot[i].task) {
			if (portslot[i].hard) {
				rt_task_delete((RT_TASK *)portslot[i].task);
			} else {
				soft_kthread_delete((RT_TASK *)portslot[i].task);
				kfree((RT_TASK *)portslot[i].task);
			}
		}
	}
	for (i = 0; i < MaxSocks; i++) {
		rt_sem_delete(&portslot[i].sem);
		soft_rt_close(portslot[i].socket[0]);
		hard_rt_close(portslot[i].socket[1]);
	}
	kfree(portslot);
	cleanup_softrtnet();
	rt_free_srq(mod_timer_srq);
	return;
}

#ifndef CONFIG_RTAI_NETRPC_BUILTIN
module_init(__rtai_netrpc_init);
module_exit(__rtai_netrpc_exit);
#endif /* !CONFIG_RTAI_NETRPC_BUILTIN */

EXPORT_SYMBOL(set_netrpc_encoding);
EXPORT_SYMBOL(rt_send_req_rel_port);
EXPORT_SYMBOL(rt_find_asgn_stub);
EXPORT_SYMBOL(rt_rel_stub);
EXPORT_SYMBOL(rt_waiting_return);
EXPORT_SYMBOL(_rt_net_rpc);
EXPORT_SYMBOL(rt_get_net_rpc_ret);
EXPORT_SYMBOL(rt_set_this_node);
EXPORT_SYMBOL(rt_set_netrpc_timeout);


#ifdef SOFT_RTNET
EXPORT_SYMBOL(soft_rt_socket);
EXPORT_SYMBOL(soft_rt_close);
EXPORT_SYMBOL(soft_rt_bind);
EXPORT_SYMBOL(soft_rt_socket_callback);
EXPORT_SYMBOL(soft_rt_sendto);
EXPORT_SYMBOL(soft_rt_recvfrom);
EXPORT_SYMBOL(ddn2nl);
#endif /* SOFT_RTNET */

EXPORT_SYMBOL(rt_net_rpc_fun_hook);
