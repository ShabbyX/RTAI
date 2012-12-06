/*
COPYRIGHT (C) 2003  Paolo Mantegazza (mantegazza@aero.polimi.it)

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


#ifndef _RTAI_SCRAMBLER_H_
#define _RTAI_SCRAMBLER_H_

#ifdef __KERNEL__

#include <rtai_sched.h>
#include <rtai_lxrt.h>
#include <rtai_netrpc.h>

#define SIZARG sizeof(arg)

extern int rt_netrpc_errenq(int port);

/*
 * it is nonsense to enquire for a netrpc error on a remote node
 * so let's have this just for uniformity of APIs
 */
#define RT_netrpc_errenq(node, port)  rt_netrpc_errenq(port)

#else

#define KEEP_STATIC_INLINE
#include <rtai_lxrt_user.h>
#include <asm/rtai_lxrt.h>
#include <rtai_lxrt.h>

extern union rtai_lxrt_t rtai_lxrt(short int dynx, short int lsize, int srq, void *arg);

#define SIZARGS sizeof(args)

/*
 * as said it is nonsense to enquire for a netrpc error on a remote node
 * but we do it this way in user space to avoid linking a call table
 * extension into LXRT just for this simple demo ...
 */
DECLARE int RT_netrpc_errenq(unsigned long node, int port)
{
	struct { int port; } arg = { port };
	struct { unsigned long node, port; long long type; void *args; int argsize; } args = { node, PACKPORT(port, 1, 1, 0), 0LL, &arg, SIZARG };
	return rtai_lxrt(NET_RPC_IDX, SIZARGS, 0, &args).i[LOW];
} 

/*
 * ... then create the illusion of a having the related non distributed API
 * also
 */
#define rt_netrpc_errenq(node, port)  RT_netrpc_errenq(node, port)

#endif

#endif
