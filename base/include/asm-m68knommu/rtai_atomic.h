/*
 * Copyright (C) 2003 Philippe Gerum <rpm@xenomai.org>
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

#ifndef _RTAI_ASM_M68KNOMMU_ATOMIC_H
#define _RTAI_ASM_M68KNOMMU_ATOMIC_H

#include <linux/autoconf.h>

#ifdef __KERNEL__

#include <linux/bitops.h>
#include <asm/atomic.h>
#include <asm/system.h>

#else /* !__KERNEL__ */

#ifndef likely
#if __GNUC__ == 2 && __GNUC_MINOR__ < 96
#define __builtin_expect(x, expected_value) (x)
#endif
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#endif /* !likely */

#define atomic_t int

struct __rtai_xchg_dummy { unsigned long a[100]; };
#define __rtai_xg(x) ((struct __rtai_xchg_dummy *)(x))

static inline unsigned long atomic_xchg(volatile void *ptr, unsigned long x)
{
    register unsigned tmp __asm__ ("%d0");
    register unsigned __ptr __asm__ ("%a1") = (unsigned)ptr;
    register unsigned long __x __asm__ ("%d2") = x;
    __asm__ __volatile__ ( "trap #13\n\t" : "+d" (tmp) : "a" (__ptr), "d" (__x) : "memory" );
    return tmp;
}

static inline unsigned long atomic_cmpxchg(volatile void *ptr, unsigned long o, unsigned long n)
{
    register unsigned prev __asm__ ("%d0");
    register unsigned __ptr __asm__ ("%a1") = (unsigned)ptr;
    register unsigned long __o __asm__ ("%d2") = o;
    register unsigned long __n __asm__ ("%d3") = n;
    __asm__ __volatile__ ( "trap #12\n\t" : "+d" (prev) : "a" (__ptr), "d" (__o), "d" (__n) : "memory" );
    return prev;
}

static __inline__ int atomic_dec_and_test(atomic_t *v)
{
    char c;
    __asm__ __volatile__("subql #1,%1; seq %0" : "=d" (c), "+m" (*v));
    return c != 0;
}

static __inline__ void atomic_inc(atomic_t *v)
{
    __asm__ __volatile__("addql #1,%0" : "+m" (*v));
}

/* Depollute the namespace a bit. */
#undef ADDR

#endif /* __KERNEL__ */

#endif /* !_RTAI_ASM_M68KNOMMU_ATOMIC_H */
