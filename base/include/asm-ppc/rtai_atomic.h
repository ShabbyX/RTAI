/*
 * Copyright (C) 2003 Philippe Gerum <rpm@xenomai.org>.
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

#ifndef _RTAI_ASM_PPC_ATOMIC_H
#define _RTAI_ASM_PPC_ATOMIC_H

#ifdef __KERNEL__

#include <linux/bitops.h>
#include <asm/system.h>
#include <asm/atomic.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
#define atomic_xchg(ptr, v)  xchg(ptr,v)
static __inline__ unsigned long atomic_cmpxchg(void *ptr, unsigned long o, unsigned long n)
{
	unsigned long *p = ptr;
	return cmpxchg(p, o, n);
}
#endif

#else /* !__KERNEL__ */

typedef struct { volatile int counter; } atomic_t;

// shamelessly taken from Linux as they are

#ifdef CONFIG_SMP
#define SMP_SYNC	"sync"
#define SMP_ISYNC	"\n\tisync"
#else
#define SMP_SYNC	""
#define SMP_ISYNC
#endif

/* Erratum #77 on the 405 means we need a sync or dcbt before every stwcx.
 * The old ATOMIC_SYNC_FIX covered some but not all of this.
 */
#ifdef CONFIG_IBM405_ERR77
#define PPC405_ERR77(ra,rb)	"dcbt " #ra "," #rb ";"
#else
#define PPC405_ERR77(ra,rb)
#endif

static __inline__ void atomic_inc(atomic_t *v)
{
	int t;

	__asm__ __volatile__(
"1:	lwarx	%0,0,%2		# atomic_inc\n\
	addic	%0,%0,1\n"
	PPC405_ERR77(0,%2)
"	stwcx.	%0,0,%2 \n\
	bne-	1b"
	: "=&r" (t), "=m" (v->counter)
	: "r" (&v->counter), "m" (v->counter)
	: "cc");
}

static __inline__ int atomic_dec_return(atomic_t *v)
{
	int t;

	__asm__ __volatile__(
"1:	lwarx	%0,0,%1		# atomic_dec_return\n\
	addic	%0,%0,-1\n"
	PPC405_ERR77(0,%1)
"	stwcx.	%0,0,%1\n\
	bne-	1b"
	SMP_ISYNC
	: "=&r" (t)
	: "r" (&v->counter)
	: "cc", "memory");

	return t;
}

#define atomic_dec_and_test(v)		(atomic_dec_return((v)) == 0)

#define __HAVE_ARCH_CMPXCHG	1

static __inline__ unsigned long
__cmpxchg_u32(volatile int *p, int old, int new)
{
	int prev;

	__asm__ __volatile__ ("\n\
1:	lwarx	%0,0,%2 \n\
	cmpw	0,%0,%3 \n\
	bne	2f \n"
	PPC405_ERR77(0,%2)
"	stwcx.	%4,0,%2 \n\
	bne-	1b\n"
#ifdef CONFIG_SMP
"	sync\n"
#endif /* CONFIG_SMP */
"2:"
	: "=&r" (prev), "=m" (*p)
	: "r" (p), "r" (old), "r" (new), "m" (*p)
	: "cc", "memory");

	return prev;
}

static __inline__ unsigned long
__cmpxchg(volatile void *ptr, unsigned long old, unsigned long new, int size)
{
	switch (size) {
	case 4:
		return __cmpxchg_u32(ptr, old, new);
#if 0	/* we don't have __cmpxchg_u64 on 32-bit PPC */
	case 8:
		return __cmpxchg_u64(ptr, old, new);
#endif /* 0 */
	}
	return old;
}

#define cmpxchg(ptr,o,n)						 \
  ({									 \
     __typeof__(*(ptr)) _o_ = (o);					 \
     __typeof__(*(ptr)) _n_ = (n);					 \
     (__typeof__(*(ptr))) __cmpxchg((ptr), (unsigned long)_o_,		 \
				    (unsigned long)_n_, sizeof(*(ptr))); \
  })

static __inline__ unsigned long atomic_cmpxchg(void *ptr, unsigned long o, unsigned long n)
{
	unsigned long *p = ptr;
	return cmpxchg(p, o, n);
}

#endif /* __KERNEL__ */

#endif /* !_RTAI_ASM_PPC_ATOMIC_H */
