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

#ifndef _VXW_SEMLIB_H_
#define _VXW_SEMLIB_H_

#include <rtai_sem.h>

#include "vxWorks.h"

typedef SEM * SEM_ID;

#define SEM_EMPTY       (0)
#define SEM_FULL        (1)
#define SEM_Q_FIFO      (FIFO_Q)
#define SEM_Q_PRIORITY  (PRIO_Q)

static inline SEM_ID semBCreate(int options, int initialState)
{
	return rt_typed_sem_init(rt_get_name(NULL), initialState, BIN_SEM | options);
}

static inline SEM_ID semCCreate(int options, int initialCount)
{
	return rt_typed_sem_init(rt_get_name(NULL), initialCount, CNT_SEM | options);
}

static inline int semDelete(SEM_ID semId)
{
	return !rt_sem_delete(semId) ? OK : ERROR;
}

static inline int semTake(SEM_ID semId, int timeout)
{
	if (timeout == NO_WAIT) {
		int retval;
		retval = rt_sem_wait_if(semId);
		return retval > 0 && retval < RTE_LOWERR ? OK : ERROR;
	}
	if (timeout == WAIT_FOREVER) {
		return abs(rt_sem_wait(semId)) < RTE_LOWERR ? OK : ERROR;
	}
	printf("*** semTake with timeout not available yet ***\n");
	return ERROR;
}

static inline int semGive(SEM_ID semId)
{
	return abs(rt_sem_signal(semId)) < RTE_LOWERR ? OK : ERROR;
}

#endif
