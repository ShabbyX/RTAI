/*
COPYRIGHT (C) 2006  Paolo Mantegazza  <mantegazza@aero.polimi.it>

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

#ifndef _VXW_MSQ_Q_LIB_H_
#define _VXW_MSG_Q_LIB_H_

#include <rtai_tbx.h>

#include "vxWorks.h"

typedef RT_MSGQ * MSG_Q_ID;

#define MSG_PRI_URGENT  (0)
#define MSG_PRI_NORMAL  (1)

static inline MSG_Q_ID msgQCreate(int maxMsgs, int maxMsgLength, int options)
{
	return rt_msgq_init(rt_get_name(NULL), maxMsgs, maxMsgLength);
}

static inline int msgQDelete(MSG_Q_ID msgQId)
{
	return !rt_msgq_delete(msgQId) ? OK : ERROR;
}

static inline int msgQSend(MSG_Q_ID msgQId, char *buffer, unsigned int nBytes, int timeout, int  priority)
{
	if (timeout == NO_WAIT || timeout == WAIT_FOREVER) {
		int retval;
		retval = timeout == NO_WAIT ? rt_msg_send_if(msgQId, buffer, nBytes, priority) : rt_msg_send_if(msgQId, buffer, nBytes, priority);
		return abs(retval) < RTE_LOWERR ? OK : ERROR;
	}
	printf("*** msgQSend with timeout not available yet ***\n");
	return ERROR;
}

static inline int msgQReceive(MSG_Q_ID msgQId, char *buffer, unsigned int  maxNBytes, int timeout)
{
	if (timeout == NO_WAIT || timeout == WAIT_FOREVER) {
		int retval;
		retval = timeout == NO_WAIT ? rt_msg_receive_if(msgQId, buffer, maxNBytes, NULL) : rt_msg_receive(msgQId, buffer, maxNBytes, NULL);
		return abs(retval) < RTE_LOWERR ? OK : ERROR;
	}
	printf("*** msgQReceive with timeout not available yet***\n");
	return ERROR;
}

#endif
