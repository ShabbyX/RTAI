/*
 *   ARTI -- RTAI-compatible Adeos-based Real-Time Interface. Based on
 *   the original RTAI layer for x86.
 *
 *   Original RTAI/x86 layer implementation:
 *   Copyright (C) 2000 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *   Copyright (C) 2000 Steve Papacharalambous <stevep@freescale.com>
 *   Copyright (C) 2000 Stuart Hughes <stuarth@lineo.com>
 *   and others.
 *
 *   RTAI/x86 rewrite over Adeos:
 *   Copyright (C) 2002 Philippe Gerum <rpm@xenomai.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _RTAI_ASM_M68K_VECTORS_H
#define _RTAI_ASM_M68K_VECTORS_H

#define RTAI_SYS_VECTOR     43
#define RTAI_CMPXCHG_TRAP_SYS_VECTOR     44
#define RTAI_XCHG_TRAP_SYS_VECTOR     45

#ifdef __KERNEL__

#include <linux/version.h>

#include <asm/ipipe.h>

#include <rtai_hal_names.h>
#include <rtai_config.h>

#endif

static inline void RTAI_DO_TRAP_SYS(long long *ret, int srq, unsigned long args)
{
	register int __srq __asm__ ("%d0") = srq;
	register unsigned long __args __asm__ ("%d1") = args;
	register unsigned long __ret1 __asm__ ("%d2");
	register unsigned long __ret2 __asm__ ("%d3");
	__asm__ __volatile__ ( "trap #11\n\t" : "+d" (__ret1),"+d" (__ret2) : "d" (__srq), "d" (__args) : "memory" );
	*ret = __ret1 + ((long long)__ret2 << 32);
}

#endif /* !_RTAI_ASM_M68K_VECTORS_H */
