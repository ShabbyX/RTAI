/*
COPYRIGHT (C) 2001-2006  Paolo Mantegazza  <mantegazza@aero.polimi.it>
			    Giuseppe Quaranta <quaranta@aero.polimi.it>

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

#ifndef _SYSAUXCLK_H_
#define _SYSAUXCLK_H_

#include <asm/rtai_srq.h>
#include <rtai_nam2num.h>

#include "vxWorks.h"

#define SINGLE_EXECUTION

#define AUX_CLK_RATE_SET      1
#define AUX_CLK_RATE_GET      2
#define AUX_CLK_CONNECT       3
#define AUX_CLK_ENABLE        4
#define AUX_CLK_DISABLE       5

#ifndef __KERNEL__

#include "taskLib.h"

#ifndef __SYS_AUX_CLK_SRQ__
#define __SYS_AUX_CLK_SRQ__

static volatile int sys_aux_clk_srq;

static int get_sys_aux_clk_srq(void)
{
	if (!sys_aux_clk_srq) {
		sys_aux_clk_srq = rtai_open_srq(nam2num("AUXCLK"));
	}
	return sys_aux_clk_srq;
}

#endif

static inline int sysAuxClkRateSet(long ticksPerSecond)
{
	struct { long code, ticks; } args = { AUX_CLK_RATE_SET, ticksPerSecond };
	return rtai_srq(get_sys_aux_clk_srq(), (unsigned long)&args);
}

static inline void sysAuxClkEnable(void) {
	struct { long code; } args = { AUX_CLK_ENABLE };
	rtai_srq(get_sys_aux_clk_srq(), (unsigned long)&args);
}

static inline void sysAuxClkDisable(void) {
	struct { long code; } args = { AUX_CLK_DISABLE };
	rtai_srq(get_sys_aux_clk_srq(), (unsigned long)&args);
}

static inline int sysAuxClkRateGet(void)
{
	struct { long code; } args = { AUX_CLK_RATE_GET };
	return rtai_srq(get_sys_aux_clk_srq(), (unsigned long)&args);
}

#ifndef  __SYS_AUX_CLOCK_FUN__
#define __SYS_AUX_CLOCK_FUN__
static void sys_aux_clock_fun(long fun, long data)
{
	struct { long code; } args = { AUX_CLK_CONNECT };
	rtai_srq(get_sys_aux_clk_srq(), (unsigned long)&args);
	while (1) {
#ifdef SINGLE_EXECUTION
		rt_task_suspend(NULL);
#else
		rt_receive(NULL, &args.code);
		if (args.code) {
			break;
		}
#endif
		((void (*)(long))fun)(data);
	}
}
#endif

static inline int sysAuxClkConnect(FUNCPTR routine, long arg) {
	if (!routine) {
		sysAuxClkDisable();
		return ERROR;
	}
	if (taskSpawn("RTCLOK", 0, 0, 0, (void *)sys_aux_clock_fun, (long)routine, arg, 0, 0, 0, 0, 0, 0, 0, 0) == ERROR) {
		return ERROR;
	}
	return OK;
}

#endif

#endif
