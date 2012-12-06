/*
 * Copyright (C) 1999-2003 Paolo Mantegazza <mantegazza@aero.polimi.it>
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


#ifndef _RTAI_ASM_I386_SRQ_H
#define _RTAI_ASM_I386_SRQ_H

#ifndef __KERNEL__

#include <sys/syscall.h>
#include <unistd.h>

#include <asm/rtai_vectors.h>

#ifdef CONFIG_RTAI_LXRT_USE_LINUX_SYSCALL
#define USE_LINUX_SYSCALL
#else
#undef USE_LINUX_SYSCALL
#endif

#define RTAI_SRQ_SYSCALL_NR  0x70000001

static inline long long rtai_srq(int srq, unsigned long args)
{
	long long retval;
#ifdef USE_LINUX_SYSCALL
        syscall(RTAI_SRQ_SYSCALL_NR, srq, args, &retval);
#else
        RTAI_DO_TRAP(RTAI_SYS_VECTOR, retval, srq, args);
#endif
	return retval;
}


static inline int rtai_open_srq(unsigned long label)
{
	return (int)rtai_srq(0, label);
}

#endif /* !__KERNEL__ */

#endif /* !_RTAI_ASM_I386_SRQ_H */
