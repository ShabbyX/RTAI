/*
 * Copyright (C) Pierre Cloutier <pcloutier@PoseidonControls.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the version 2 of the GNU Lesser
 * General Public License as published by the Free Software
 * Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */


/* dummy defines to avoid annoying warning about here unused stuff */
#define read_cr4()  0
#define write_cr4(x)
/* end of dummy defines to avoid annoying warning about here unused stuff */

#define CONFIG_RTAI_LXRT_EXTERN_INLINE

extern inline union rtai_lxrt_t rtai_lxrt(short int dynx, short int lsize, int srq, void *arg);

#include <rtai_lxrt.h>
#include <rtai_signal.h>
#include <rtai_version.h>
#include <rtai_sched.h>
#include <rtai_malloc.h>
#include <rtai_trace.h>
#include <rtai_leds.h>
#include <rtai_sem.h>
#include <rtai_rwl.h>
#include <rtai_spl.h>
#include <rtai_scb.h>
#include <rtai_mbx.h>
#include <rtai_msg.h>
#include <rtai_tbx.h>
#include <rtai_mq.h>
#include <rtai_bits.h>
#include <rtai_wd.h>
#include <rtai_tasklets.h>
#include <rtai_fifos.h>
#include <rtai_netrpc.h>
#include <rtai_shm.h>
#include <rtai_usi.h>
#include <rtai_posix.h>
#ifdef CONFIG_RTAI_DRIVERS_SERIAL
#include <rtai_serial.h>
#endif /* CONFIG_RTAI_DRIVERS_SERIAL */
#ifdef CONFIG_RTAI_TASKLETS
#include <rtai_tasklets.h>
#endif /* CONFIG_RTAI_TASKLETS */

#include <unistd.h>
#include <sys/mman.h>

void rtai_linux_syscall_server_fun(struct linux_syscalls_list *list)
{
	struct linux_syscalls_list syscalls;

	syscalls = *list;
	syscalls.serv = &syscalls;
	if ((syscalls.serv = rtai_lxrt(BIDX, sizeof(struct linux_syscalls_list), LINUX_SERVER, &syscalls).v[LOW])) {
		long *args;
		struct linux_syscall *todo;
		struct linux_syscall calldata[syscalls.nr];
		syscalls.syscall = calldata;
		memset(calldata, 0, sizeof(calldata));
                mlockall(MCL_CURRENT | MCL_FUTURE);
		list->serv = &syscalls;
		rtai_lxrt(BIDX, sizeof(RT_TASK *), RESUME, &syscalls.task);
		while (abs(rtai_lxrt(BIDX, sizeof(RT_TASK *), SUSPEND, &syscalls.serv).i[LOW]) < RTE_LOWERR) {
			if (syscalls.syscall[syscalls.out].mode != LINUX_SYSCALL_CANCELED) {
				todo = &syscalls.syscall[syscalls.out];
				args = todo->args;
				todo->retval = syscall(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
				todo->id = -todo->id;
				if (todo->mode == SYNC_LINUX_SYSCALL) {
					rtai_lxrt(BIDX, sizeof(RT_TASK *), RESUME, &syscalls.task);
				} else if (syscalls.cbfun) {
					todo->cbfun(args[0], todo->retval);
				}
			}
			if (++syscalls.out >= syscalls.nr) {
				syscalls.out = 0;
			}
		}
        }
	rtai_lxrt(BIDX, sizeof(RT_TASK *), LXRT_TASK_DELETE, &syscalls.serv);
}
