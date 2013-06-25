/*
 * Copyright (C) 2005 Paolo Mantegazza <mantegazza@aero.polimi.it>
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


#ifndef _RTAI_FUSION_LXRT_H
#define _RTAI_FUSION_LXRT_H

#include <types.h>

#define RT_EINTR      (0xFff0)
#define RT_EIDRM      (0xFfff)
#define RT_ETIMEDOUT  (0xFffe)

// scheduler
#define YIELD				 0
#define SUSPEND				 1
#define RESUME				 2
#define MAKE_PERIODIC 			 3
#define WAIT_PERIOD	 		 4
#define SLEEP		 		 5
#define SLEEP_UNTIL	 		 6
#define START_TIMER	 		 7
#define STOP_TIMER	 		 8
#define GET_TIME	 		 9
#define COUNT2NANO			10
#define NANO2COUNT			11
#define BUSY_SLEEP			12
#define SET_PERIODIC_MODE		13
#define SET_ONESHOT_MODE		14
#define SIGNAL_HANDLER	 		15
#define TASK_USE_FPU			16
#define LINUX_USE_FPU			17
#define PREEMPT_ALWAYS_GEN		18
#define GET_TIME_NS			19
#define GET_CPU_TIME_NS			20
#define SET_RUNNABLE_ON_CPUS		21
#define SET_RUNNABLE_ON_CPUID		22
#define GET_TIMER_CPU			23
#define START_RT_APIC_TIMERS		24
#define PREEMPT_ALWAYS_CPUID		25
#define COUNT2NANO_CPUID		26
#define NANO2COUNT_CPUID		27
#define GET_TIME_CPUID			28
#define GET_TIME_NS_CPUID       	29
#define MAKE_PERIODIC_NS 		30
#define SET_SCHED_POLICY 		31
#define SET_RESUME_END 			32
#define SPV_RMS 			33
#define WAKEUP_SLEEPING			34
#define CHANGE_TASK_PRIO 		35
#define SET_RESUME_TIME 		36
#define SET_PERIOD        		37
#define HARD_TIMER_RUNNING              38

// semaphores
#define TYPED_SEM_INIT 			39
#define SEM_DELETE			40
#define NAMED_SEM_INIT 			41
#define NAMED_SEM_DELETE		42
#define SEM_SIGNAL			43
#define SEM_WAIT			44
#define SEM_WAIT_IF			45
#define SEM_WAIT_UNTIL			46
#define SEM_WAIT_TIMED			47
#define SEM_BROADCAST       		48
#define SEM_WAIT_BARRIER 		49
#define SEM_COUNT			50
#define COND_WAIT			51
#define COND_WAIT_UNTIL			52
#define COND_WAIT_TIMED			53
#define RWL_INIT 			54
#define RWL_DELETE			55
#define NAMED_RWL_INIT 			56
#define NAMED_RWL_DELETE		57
#define RWL_RDLOCK 			58
#define RWL_RDLOCK_IF 			59
#define RWL_RDLOCK_UNTIL 		60
#define RWL_RDLOCK_TIMED 		61
#define RWL_WRLOCK 			62
#define RWL_WRLOCK_IF	 		63
#define RWL_WRLOCK_UNTIL		64
#define RWL_WRLOCK_TIMED		65
#define RWL_UNLOCK 			66
#define SPL_INIT 			67
#define SPL_DELETE			68
#define NAMED_SPL_INIT 			69
#define NAMED_SPL_DELETE		70
#define SPL_LOCK 			71
#define SPL_LOCK_IF	 		72
#define SPL_LOCK_TIMED			73
#define SPL_UNLOCK 			74

// mail boxes
#define TYPED_MBX_INIT 			75
#define MBX_DELETE			76
#define NAMED_MBX_INIT 			77
#define NAMED_MBX_DELETE		78
#define MBX_SEND			79
#define MBX_SEND_WP 			80
#define MBX_SEND_IF 			81
#define MBX_SEND_UNTIL			82
#define MBX_SEND_TIMED			83
#define MBX_RECEIVE 			84
#define MBX_RECEIVE_WP 			85
#define MBX_RECEIVE_IF 			86
#define MBX_RECEIVE_UNTIL		87
#define MBX_RECEIVE_TIMED		88
#define MBX_EVDRP			89
#define MBX_OVRWR_SEND                  90

// short intertask messages
#define SENDMSG				91
#define SEND_IF				92
#define SEND_UNTIL			93
#define SEND_TIMED			94
#define RECEIVEMSG			95
#define RECEIVE_IF			96
#define RECEIVE_UNTIL			97
#define RECEIVE_TIMED			98
#define RPCMSG				99
#define RPC_IF			       100
#define RPC_UNTIL		       101
#define RPC_TIMED		       102
#define EVDRP			       103
#define ISRPC			       104
#define RETURNMSG		       105

// extended intertask messages
#define RPCX			       106
#define RPCX_IF			       107
#define RPCX_UNTIL		       108
#define RPCX_TIMED		       109
#define SENDX			       110
#define SENDX_IF		       111
#define SENDX_UNTIL		       112
#define SENDX_TIMED		       113
#define RETURNX			       114
#define RECEIVEX		       115
#define RECEIVEX_IF		       116
#define RECEIVEX_UNTIL		       117
#define RECEIVEX_TIMED		       118
#define EVDRPX			       119

// proxies
#define PROXY_ATTACH                   120
#define PROXY_DETACH     	       121
#define PROXY_TRIGGER                  122


// synchronous user space specific intertask messages and related proxies
#define RT_SEND                        123
#define RT_RECEIVE                     124
#define RT_CRECEIVE          	       125
#define RT_REPLY                       126
#define RT_PROXY_ATTACH                127
#define RT_PROXY_DETACH                128
#define RT_TRIGGER                     129
#define RT_NAME_ATTACH                 130
#define RT_NAME_DETACH                 131
#define RT_NAME_LOCATE                 132

// bits
#define BITS_INIT          	       133
#define BITS_DELETE        	       134
#define NAMED_BITS_INIT    	       135
#define NAMED_BITS_DELETE  	       136
#define BITS_GET           	       137
#define BITS_RESET         	       138
#define BITS_SIGNAL        	       139
#define BITS_WAIT          	       140
#define BITS_WAIT_IF       	       141
#define BITS_WAIT_UNTIL    	       142
#define BITS_WAIT_TIMED   	       143

// typed mail boxes
#define TBX_INIT                       144
#define TBX_DELETE         	       145
#define NAMED_TBX_INIT                 146
#define NAMED_TBX_DELETE               147
#define TBX_SEND                       148
#define TBX_SEND_IF                    149
#define TBX_SEND_UNTIL                 150
#define TBX_SEND_TIMED                 151
#define TBX_RECEIVE                    152
#define TBX_RECEIVE_IF                 153
#define TBX_RECEIVE_UNTIL              154
#define TBX_RECEIVE_TIMED              155
#define TBX_BROADCAST                  156
#define TBX_BROADCAST_IF               157
#define TBX_BROADCAST_UNTIL            158
#define TBX_BROADCAST_TIMED            159
#define TBX_URGENT                     160
#define TBX_URGENT_IF                  161
#define TBX_URGENT_UNTIL               162
#define TBX_URGENT_TIMED               163

// pqueue
#define MQ_OPEN         	       164
#define MQ_RECEIVE      	       165
#define MQ_SEND         	       166
#define MQ_CLOSE        	       167
#define MQ_GETATTR     		       168
#define MQ_SETATTR      	       169
#define MQ_NOTIFY       	       170
#define MQ_UNLINK       	       171
#define MQ_TIMEDRECEIVE 	       172
#define MQ_TIMEDSEND    	       173

// named tasks init/delete
#define NAMED_TASK_INIT 	       174
#define NAMED_TASK_INIT_CPUID 	       175
#define NAMED_TASK_DELETE	       176

// registry
#define GET_ADR         	       177
#define GET_NAME         	       178

// netrpc
#define NETRPC			       179
#define SEND_REQ_REL_PORT	       180
#define DDN2NL			       181
#define SET_THIS_NODE		       182
#define FIND_ASGN_STUB		       183
#define REL_STUB		       184
#define WAITING_RETURN		       185

// a semaphore extension
#define COND_SIGNAL		       186

// new shm
#define SHM_ALLOC                      187
#define SHM_FREE                       188
#define SHM_SIZE                       189
#define HEAP_SET                       190
#define HEAP_ALLOC                     191
#define HEAP_FREE                      192
#define HEAP_NAMED_ALLOC               193
#define HEAP_NAMED_FREE                194
#define MALLOC                         195
#define FREE                           196
#define NAMED_MALLOC                   197
#define NAMED_FREE                     198

#define SUSPEND_IF		       199
#define SUSPEND_UNTIL	 	       200
#define SUSPEND_TIMED		       201
#define IRQ_WAIT		       202
#define IRQ_WAIT_IF		       203
#define IRQ_WAIT_UNTIL		       204
#define IRQ_WAIT_TIMED		       205
#define IRQ_SIGNAL		       206
#define REQUEST_IRQ_TASK	       207
#define RELEASE_IRQ_TASK	       208
#define SCHED_LOCK		       209
#define SCHED_UNLOCK		       210
#define PEND_LINUX_IRQ		       211
#define RECEIVE_LINUX_SYSCALL          212
#define RETURN_LINUX_SYSCALL           213

#define MAX_LXRT_FUN                   215

// not recovered yet
// Qblk's
#define RT_INITTICKQUEUE		69
#define RT_RELEASETICKQUEUE     	70
#define RT_QDYNALLOC            	71
#define RT_QDYNFREE             	72
#define RT_QDYNINIT             	73
#define RT_QBLKWAIT			74
#define RT_QBLKREPEAT			75
#define RT_QBLKSOON			76
#define RT_QBLKDEQUEUE			77
#define RT_QBLKCANCEL			78
#define RT_QSYNC			79
#define RT_QRECEIVE			80
#define RT_QLOOP			81
#define RT_QSTEP			82
#define RT_QBLKBEFORE			83
#define RT_QBLKAFTER			84
#define RT_QBLKUNHOOK			85
#define RT_QBLKRELEASE			86
#define RT_QBLKCOMPLETE			87
#define RT_QHOOKFLUSH			88
#define RT_QBLKATHEAD			89
#define RT_QBLKATTAIL			90
#define RT_QHOOKINIT			91
#define RT_QHOOKRELEASE			92
#define RT_QBLKSCHEDULE			93
#define RT_GETTICKQUEUEHOOK		94
// Testing
#define RT_BOOM				95
#define RTAI_MALLOC			96
#define RT_FREE				97
#define RT_MMGR_STATS			98
#define RT_STOMP                	99
// VC
#define RT_VC_ATTACH            	100
#define RT_VC_RELEASE           	101
#define RT_VC_RESERVE          		102
// Linux Signal Support
#define RT_GET_LINUX_SIGNAL		103
#define RT_GET_ERRNO			104
#define RT_SET_LINUX_SIGNAL_HANDLER	105
// end of not recovered yet

#define LXRT_GET_ADR		1000
#define LXRT_GET_NAME   	1001
#define LXRT_TASK_INIT 		1002
#define LXRT_TASK_DELETE 	1003
#define LXRT_SEM_INIT  		1004
#define LXRT_SEM_DELETE		1005
#define LXRT_MBX_INIT 		1006
#define LXRT_MBX_DELETE		1007
#define MAKE_SOFT_RT		1008
#define MAKE_HARD_RT		1009
#define PRINT_TO_SCREEN		1010
#define NONROOT_HRT		1011
#define RT_BUDDY		1012
#define HRT_USE_FPU     	1013
#define USP_SIGHDL      	1014
#define GET_USP_FLAGS   	1015
#define SET_USP_FLAGS   	1016
#define GET_USP_FLG_MSK 	1017
#define SET_USP_FLG_MSK 	1018
#define IS_HARD         	1019
#define LINUX_SERVER_INIT	1020
#define ALLOC_REGISTER 		1021
#define DELETE_DEREGISTER	1022
#define FORCE_TASK_SOFT  	1023
#define PRINTK			1024
#define GET_EXECTIME		1025
#define GET_TIMEORIG 		1026
#define LXRT_RWL_INIT		1027
#define LXRT_RWL_DELETE 	1028
#define LXRT_SPL_INIT		1029
#define LXRT_SPL_DELETE 	1030

// Keep LXRT call enc/decoding together, so you are sure to act consistently.
// This is the encoding, note " | GT_NR_SYSCALLS" to ensure not a Linux syscall, ...
#define GT_NR_SYSCALLS  (1 << 11)
#define ENCODE_LXRT_REQ(dynx, srq, lsize)  (((dynx) << 24) | ((srq) << 12) | GT_NR_SYSCALLS | (lsize))

#include <asm/rtai_lxrt.h>

#define BIDX    0 // rt_fun_ext[0]
#define SIZARG  sizeof(arg)

static inline void *rt_malloc(int size)
{
	struct { long size; } arg = { size };
	return rtai_lxrt(BIDX, SIZARG, MALLOC, &arg).v[LOW];
}

static inline void rt_free(void *addr)
{
	struct { void *addr; } arg = { addr };
	rtai_lxrt(BIDX, SIZARG, FREE, &arg);
}

static inline void *rt_get_handle(unsigned long id)
{
	struct { unsigned long id; } arg = { id };
	return (void *)rtai_lxrt(BIDX, SIZARG, GET_ADR, &arg).v[LOW];
}

static inline unsigned long nam2id (const char *name)
{
	unsigned long retval = 0;
	int c, i;

	for (i = 0; i < 6; i++) {
		if (!(c = name[i]))
			break;
		if (c >= 'a' && c <= 'z') {
			c +=  (11 - 'a');
		} else if (c >= 'A' && c <= 'Z') {
			c += (11 - 'A');
		} else if (c >= '0' && c <= '9') {
			c -= ('0' - 1);
		} else {
			c = c == '_' ? 37 : 38;
		}
		retval = retval*39 + c;
	}
	if (i > 0)
		return retval + 2;
	else
		return 0xFFFFFFFF;
}

static inline void rt_release_waiters(unsigned long id)
{
	struct { void *sem; } arg;
	if ((arg.sem = rt_get_handle(~id))) {
		rtai_lxrt(BIDX, SIZARG, SEM_BROADCAST, &arg);
	}
}


static inline int rt_obj_bind(void *p, const char *name)
{
	struct { void *adr; } *obj = p;
	unsigned long id = nam2id(name);
	if ((obj->adr = rt_get_handle(id))) {
		return 0;
	} else {
		struct { void *sem; } sem;
		if (!(sem.sem = rt_get_handle(~id))) {
			struct { unsigned long name; long value, type; } arg = { id, 0, CNT_SEM };
			if (!(sem.sem = rtai_lxrt(BIDX, SIZARG, NAMED_SEM_INIT, &arg).v[LOW])) {
				return -EFAULT;
			}
		}
		if ((rtai_lxrt(BIDX, sizeof(sem), SEM_WAIT, &sem).i[LOW] >= SEM_TIMOUT)) {
			return -EFAULT;
		}
	}
	obj->adr = rt_get_handle(id);
	return 0;
}

static inline int rt_obj_unbind(void *p)
{
	struct { void *adr; } *obj = p;
	obj->adr = NULL;
	return 0;
}

#define rt_print_auto_init(a)

#define rt_printf rt_printk

int rt_printk(const char *format, ...);

#endif /* !_RTAI_FUSION_LXRT_H */
